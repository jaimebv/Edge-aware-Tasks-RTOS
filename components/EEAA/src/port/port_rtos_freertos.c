/**
 * @file port_rtos_freertos.c
 * @brief FreeRTOS Scheduler Abstraction Layer Implementation
 *
 * This module implements portable wrappers around FreeRTOS APIs.
 * These implementations are completely independent of the underlying
 * hardware and can be used with any FreeRTOS port (ESP32, STM32, ARM, etc.).
 *
 * Implementation Details:
 * - Abstracts FreeRTOS-specific types and operations
 * - Provides RTOS-agnostic interface via port_interface_types.h
 * - Maps FreeRTOS types to abstract types for portability
 * - Handles FreeRTOS semaphore creation/destruction for mutexes
 * - Converts FreeRTOS task states to abstract task states
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
 */

#include "port/port_rtos_freertos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/*===========================================================================*/
/* MEMORY MANAGEMENT                                                         */
/*===========================================================================*/

void* eaPort_Malloc(size_t size)
{
    return pvPortMalloc(size);
}


void eaPort_Free(void* ptr)
{
    if (ptr != NULL) {
        vPortFree(ptr);
    }
}


/*===========================================================================*/
/* SYNCHRONIZATION PRIMITIVES                                                */
/*===========================================================================*/

eaPort_mutex_t eaPort_Mutex_Create(void)
{
    /* Create a FreeRTOS binary semaphore to act as a mutex.
     * xSemaphoreCreateMutex() creates a semaphore with initial count of 1. */
    return (eaPort_mutex_t)xSemaphoreCreateMutex();
}


void eaPort_Mutex_Destroy(eaPort_mutex_t mutex)
{
    /* Safely destroy the semaphore handle.
     * In FreeRTOS, vSemaphoreDelete() handles NULL gracefully. */
    if (mutex != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex);
    }
}


void eaPort_Mutex_Enter(eaPort_mutex_t mutex)
{
    /* Acquire the mutex with indefinite timeout.
     * xSemaphoreTake blocks until the mutex is available.
     * portMAX_DELAY means wait forever. */
    if (mutex != NULL) {
        xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY);
    }
}


void eaPort_Mutex_Exit(eaPort_mutex_t mutex)
{
    /* Release the mutex.
     * xSemaphoreGive increments the semaphore count, allowing other tasks to acquire it. */
    if (mutex != NULL) {
        xSemaphoreGive((SemaphoreHandle_t)mutex);
    }
}


/*===========================================================================*/
/* QUEUE MANAGEMENT IMPLEMENTATION                                           */
/*===========================================================================*/

eaPort_queue_t eaPort_Queue_Create(uint32_t queue_length, uint32_t item_size)
{
    /* Wrapper for xQueueCreate */
    return (eaPort_queue_t)xQueueCreate((UBaseType_t)queue_length, (UBaseType_t)item_size);
}


void eaPort_Queue_Delete(eaPort_queue_t queue)
{
    if (queue != NULL) {
        vQueueDelete((QueueHandle_t)queue);
    }
}


eaPort_status_t eaPort_Queue_Send(eaPort_queue_t queue, const void *item, uint32_t wait_ms)
{
    if (queue == NULL || item == NULL) {
        return eaPort_STATUS_ERROR;
    }

    TickType_t ticks;
    if (wait_ms == eaPort_WAIT_FOREVER) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(wait_ms);
    }

    /* Wrapper for xQueueSend (pushes to back) */
    BaseType_t result = xQueueSend((QueueHandle_t)queue, item, ticks);
    
    return (result == pdPASS) ? eaPort_STATUS_OK : eaPort_STATUS_ERROR;
}


eaPort_status_t eaPort_Queue_Receive(eaPort_queue_t queue, void *buffer, uint32_t wait_ms)
{
    if (queue == NULL || buffer == NULL) {
        return eaPort_STATUS_ERROR;
    }

    TickType_t ticks;
    if (wait_ms == eaPort_WAIT_FOREVER) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(wait_ms);
    }

    BaseType_t result = xQueueReceive((QueueHandle_t)queue, buffer, ticks);

    return (result == pdPASS) ? eaPort_STATUS_OK : eaPort_STATUS_ERROR;
}


uint32_t eaPort_Queue_Messages_Waiting(eaPort_queue_t queue)
{
    if (queue == NULL) return 0;
    return (uint32_t)uxQueueMessagesWaiting((QueueHandle_t)queue);
}


/*===========================================================================*/
/* TASK LIFECYCLE MANAGEMENT                                                 */
/*===========================================================================*/


eaPort_status_t eaPort_Task_Create_Pinned_to_Core(
    eaPort_task_function_t pxTaskCode,
    const char *pcName,
    uint32_t usStackDepth,
    void *pvParameters,
    uint32_t uxPriority,
    eaPort_task_t *pxCreatedTask,
    int32_t xCoreID
)
{
    /* Create task using FreeRTOS xTaskCreatePinnedToCore.
     * This function creates a task pinned to a specific core on multi-core systems.
     * On single-core systems, xCoreID is ignored.
     *
     * Parameter mapping:
     * - pxTaskCode (eaPort_task_function_t) → TaskFunction_t (FreeRTOS)
     * - pcName → task name string
     * - usStackDepth → stack size in words
     * - pvParameters → task parameter
     * - uxPriority → task priority (0 = lowest)
     * - pxCreatedTask → output task handle (can be NULL)
     * - xCoreID → core affinity (tskNO_AFFINITY = no specific core)
     */

    BaseType_t freertosCoreID;

    if (xCoreID == eaPort_NO_AFFINITY) {
        freertosCoreID = tskNO_AFFINITY;
    } else {
        freertosCoreID = (BaseType_t)xCoreID;
    }
    
    BaseType_t result = xTaskCreatePinnedToCore(
        (TaskFunction_t)pxTaskCode,
        pcName,
        usStackDepth,
        pvParameters,
        (UBaseType_t)uxPriority,
        (TaskHandle_t*)pxCreatedTask,
        (BaseType_t)freertosCoreID
    );
    
    /* Convert FreeRTOS return value to abstract status code */
    return (result == pdPASS) ? eaPort_STATUS_OK : eaPort_STATUS_ERROR;
}


void eaPort_Task_Delete(eaPort_task_t xTaskToDelete)
{
    /* Delete the specified task or current task if NULL.
     * Frees all resources associated with the task. */
    vTaskDelete((TaskHandle_t)xTaskToDelete);
}


void eaPort_Task_Suspend(eaPort_task_t xTaskToSuspend)
{
    /* Suspend the specified task or current task if NULL.
     * A suspended task uses no CPU time but retains its state. */
    vTaskSuspend((TaskHandle_t)xTaskToSuspend);
}


void eaPort_Task_Resume(eaPort_task_t xTaskToResume)
{
    /* Resume a previously suspended task.
     * The task becomes eligible to run again. */
    vTaskResume((TaskHandle_t)xTaskToResume);
}


/*===========================================================================*/
/* TASK STATE RETRIEVAL AND MONITORING                                       */
/*===========================================================================*/

/**
 * @brief Helper function to convert FreeRTOS task state to abstract task state.
 * Maps FreeRTOS eTaskState enum values to RTOS-agnostic eaPort_task_state_t values.
 * @param[in] freertosState FreeRTOS task state (eRunning, eReady, etc.)
 * @return eaPort_task_state_t Abstract task state.
 */
static eaPort_task_state_t freertos_convert_task_state(eTaskState freertosState)
{
    switch (freertosState) {
        case eRunning:
            return eaPort_TaskState_Running;
        case eReady:
            return eaPort_TaskState_Ready;
        case eBlocked:
            return eaPort_TaskState_Blocked;
        case eSuspended:
            return eaPort_TaskState_Suspended;
        case eDeleted:
            return eaPort_TaskState_Deleted;
        default:
            return eaPort_TaskState_Unknown;
    }
}



eaPort_status_t eaPort_Get_Task_Info (eaPort_task_info_t *taskInfo, eaPort_task_t *taskHandle)
{
    
    if (taskInfo == NULL || taskHandle == NULL) {
        eaPort_task_info_t emptyInfo = {0};
        *taskInfo = emptyInfo;
        return eaPort_STATUS_ERROR;
    }

    TaskHandle_t handle = (TaskHandle_t)(*taskHandle);
    

    /* Retrieve FreeRTOS task information */
    #if configUSE_TRACE_FACILITY == 1
        TaskStatus_t freertosStatus;
        /* If trace facility is enabled, we can get more detailed info */
        vTaskGetInfo(handle, &freertosStatus, pdTRUE, eInvalid);


        /* Populate the abstract task info structure */
        taskInfo->xTaskHandle = (eaPort_task_t)freertosStatus.xHandle;
        taskInfo->pcTaskName = freertosStatus.pcTaskName;
        taskInfo->ulRunTimeCounter = freertosStatus.ulRunTimeCounter;
        taskInfo->uxTaskNumber = freertosStatus.xTaskNumber;
        taskInfo->eCurrentState = freertos_convert_task_state(freertosStatus.eCurrentState);
        taskInfo->uxCurrentPriority = freertosStatus.uxCurrentPriority;
        taskInfo->uxStackHighWaterMark = freertosStatus.usStackHighWaterMark;
    #else
        /* If trace facility is not enabled, provide limited info */
        taskInfo->xTaskHandle = (eaPort_task_t)handle;
        taskInfo->pcTaskName = pcTaskGetName(handle);
        taskInfo->ulRunTimeCounter = 0;  /* Not available */
        taskInfo->uxTaskNumber = 0;      /* Not available */
        taskInfo->eCurrentState = eaPort_TaskState_Unknown; /* Not available */
        taskInfo->uxCurrentPriority = uxTaskPriorityGet(handle);
        taskInfo->uxStackHighWaterMark = uxTaskGetStackHighWaterMark(handle);
    #endif
    if (taskInfo->pcTaskName == NULL) {
        taskInfo->pcTaskName = "";
        return eaPort_STATUS_ERROR;
    }
    return eaPort_STATUS_OK;
}

eaPort_task_t eaPort_Get_Current_Task_Handle()
{
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    return (eaPort_task_t)handle;
}

eaPort_task_t eaPort_Get_Task_Handle_By_Name(const char* taskName)
{
    if (taskName == NULL) {
        return NULL;
    }

    TaskHandle_t handle = xTaskGetHandle(taskName);
    return (eaPort_task_t)handle;
}

// eaPort_task_info_t* eaPort_Get_Task_Status_Array(uint32_t *numTasks, uint32_t *totalRunTime)
// {
//     if (numTasks == NULL) {
//         return NULL;
//     }

//     /* Get the current number of tasks in the system */
//     UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    
//     /* Allocate memory for the FreeRTOS task status array */
//     TaskStatus_t *freertosArray = (TaskStatus_t *)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
    
//     if (freertosArray == NULL) {
//         return NULL;
//     }

//     /* Capture task states into FreeRTOS array */
//     UBaseType_t actualTaskCount = uxTaskGetSystemState(freertosArray, taskCount, 
//                                                        (uint32_t *)totalRunTime);
    
//     /* Allocate memory for the abstract task info array */
//     eaPort_task_info_t *abstractArray = (eaPort_task_info_t *)pvPortMalloc(
//         actualTaskCount * sizeof(eaPort_task_info_t)
//     );
    
//     if (abstractArray == NULL) {
//         vPortFree(freertosArray);
//         return NULL;
//     }

//     /* Convert FreeRTOS task statuses to abstract task info */
//     for (UBaseType_t i = 0; i < actualTaskCount; i++) {
//         abstractArray[i].xTaskHandle = (eaPort_task_t)freertosArray[i].xHandle;
//         abstractArray[i].pcTaskName = freertosArray[i].pcTaskName;
//         abstractArray[i].ulRunTimeCounter = freertosArray[i].ulRunTimeCounter;
//         abstractArray[i].uxTaskNumber = freertosArray[i].xTaskNumber;
//         abstractArray[i].eCurrentState = freertos_convert_task_state(freertosArray[i].eCurrentState);
//         abstractArray[i].uxCurrentPriority = freertosArray[i].uxCurrentPriority;
//         abstractArray[i].uxStackHighWaterMark = freertosArray[i].usStackHighWaterMark;
//     }

//     /* Free the temporary FreeRTOS array */
//     vPortFree(freertosArray);

//     *numTasks = actualTaskCount;
//     return abstractArray;
// }


uint32_t eaPort_Get_Task_Runtime_Counter(const eaPort_task_info_t* taskStatus)
{
    if (taskStatus == NULL) {
        return 0;
    }
    
    return taskStatus->ulRunTimeCounter;
}


uint32_t eaPort_Get_Task_Runtime_Percentage(const eaPort_task_info_t* taskStatus, uint32_t totalRunTimeDiv100)
{
    if ((taskStatus == NULL) || (totalRunTimeDiv100 == 0)) {
        return 0;
    }
    
    return taskStatus->ulRunTimeCounter / totalRunTimeDiv100;
}


const char* eaPort_Get_Task_Name(const eaPort_task_info_t* taskStatus)
{
    if (taskStatus == NULL) {
        return "";
    }
    
    return taskStatus->pcTaskName;
}


uint32_t eaPort_Get_Task_Priority(const eaPort_task_info_t* taskStatus)
{
    if (taskStatus == NULL) {
        return 0;
    }
    
    return taskStatus->uxCurrentPriority;
}


const char* eaPort_Get_Task_State_str(const eaPort_task_info_t* taskStatus)
{
    if (taskStatus == NULL) {
        return "Unknown";
    }
    
    switch (taskStatus->eCurrentState) {
        case eaPort_TaskState_Running:
            return "Running";
        case eaPort_TaskState_Ready:
            return "Ready";
        case eaPort_TaskState_Blocked:
            return "Blocked";
        case eaPort_TaskState_Suspended:
            return "Suspended";
        case eaPort_TaskState_Deleted:
            return "Deleted";
        default:
            return "Unknown";
    }
}


char eaPort_Get_Task_State_char(const eaPort_task_info_t* taskStatus)
{
    if (taskStatus == NULL) {
        return 'U';
    }
    
    switch (taskStatus->eCurrentState) {
        case eaPort_TaskState_Running:
            return 'R';
        case eaPort_TaskState_Ready:
            return 'E';
        case eaPort_TaskState_Blocked:
            return 'B';
        case eaPort_TaskState_Suspended:
            return 'S';
        case eaPort_TaskState_Deleted:
            return 'D';
        default:
            return 'U';
    }
}


/*===========================================================================*/
/* SYSTEM METRICS AND TIMING                                                 */
/*===========================================================================*/

eaPort_tick_t eaPort_Get_Tick_Time(void)
{
    return (eaPort_tick_t)xTaskGetTickCount();
}

void eaPort_Delay_Milliseconds(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void eaPort_Delay_Until(eaPort_tick_t *previousWakeTime, uint32_t timeIncrementMs)
{
    vTaskDelayUntil((TickType_t *)previousWakeTime, pdMS_TO_TICKS(timeIncrementMs));
}


// void eaPort_Get_Task_Runtime_Stats(char *pcWriteBuffer)
// {
//     if (pcWriteBuffer != NULL) {
//         vTaskGetRunTimeStats(pcWriteBuffer);
//     }
// }


// void eaPort_Get_Task_List(char *pcWriteBuffer)
// {
//     if (pcWriteBuffer != NULL) {
//         vTaskList(pcWriteBuffer);
//     }
// }
