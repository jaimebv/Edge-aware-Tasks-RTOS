/**
 * @file port_freertos.h
 * @brief FreeRTOS Scheduler Abstraction Layer
 *
 * This module provides a hardware-independent abstraction layer for FreeRTOS
 * scheduler operations. It encapsulates task management, memory operations,
 * synchronization primitives, and system metrics that are portable across
 * all platforms using FreeRTOS.
 *
 * Type Mapping:
 * - eaPort_task_t → TaskHandle_t (FreeRTOS task handle)
 * - eaPort_mutex_t → SemaphoreHandle_t (FreeRTOS semaphore/mutex)
 * - eaPort_task_state_t → eTaskState (mapped to FreeRTOS task states)
 * - eaPort_task_info_t → TaskStatus_t (mapped from FreeRTOS TaskStatus_t)
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
 *
 */

#ifndef PORT_FREERTOS_H
#define PORT_FREERTOS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "port_interface_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* FREERTOS-SPECIFIC TYPE MAPPINGS                                          */
/*===========================================================================*/

/*
 * NOTE: Type mappings are handled via void* pointers in port_interface_types.h.
 * The abstract types (eaPort_task_t, eaPort_mutex_t) are void* and are cast
 * to FreeRTOS-specific types as needed. This avoids conflicting typedef.
 */

/*===========================================================================*/
/* MEMORY MANAGEMENT - FreeRTOS Abstraction                                  */
/*===========================================================================*/

/**
 * @brief Allocates memory from the FreeRTOS heap.
 * Portable wrapper around FreeRTOS memory allocation (pvPortMalloc).
 * Uses the underlying FreeRTOS heap implementation.
 * @param[in] size Number of bytes to allocate.
 * @return void* Pointer to allocated memory, or NULL if allocation fails.
 * @note Must be paired with eaPort_Free().
 */
void* eaPort_Malloc(size_t size);


/**
 * @brief Frees memory allocated by eaPort_Malloc().
 * Portable wrapper around FreeRTOS memory deallocation (vPortFree).
 * @param[in] ptr Pointer to memory previously allocated by eaPort_Malloc().
 * Passing NULL is safe and has no effect.
 * @note Must be paired with eaPort_Malloc().
 */
void eaPort_Free(void* ptr);


/*===========================================================================*/
/* SYNCHRONIZATION PRIMITIVES - FreeRTOS Abstraction                         */
/*===========================================================================*/

/**
 * @brief Creates a new mutex.
 * Allocates and initializes a FreeRTOS binary semaphore as a mutex.
 * @return eaPort_mutex_t Handle to the mutex, or NULL if creation fails.
 * @note The returned handle must be freed with eaPort_Mutex_Destroy().
 */
eaPort_mutex_t eaPort_Mutex_Create(void);


/**
 * @brief Destroys a FreeRTOS mutex.
 * Releases resources associated with the mutex.
 * @param[in] mutex Mutex handle returned by eaPort_Mutex_Create().
 *                  Passing NULL is safe and has no effect.
 * @note Do not use the mutex after calling this function.
 */
void eaPort_Mutex_Destroy(eaPort_mutex_t mutex);


/**
 * @brief Acquires exclusive access to a critical section.
 * Enters a critical section using FreeRTOS mutex (binary semaphore).
 * Only one task can hold the lock at a time.
 * @param[in] mutex Mutex handle previously created or initialized.
 * @note This function blocks until the mutex is acquired.
 * @note Do NOT call from interrupt handlers.
 * @warning Always match with eaPort_Mutex_Exit().
 */
void eaPort_Mutex_Enter(eaPort_mutex_t mutex);


/**
 * @brief Releases exclusive access to a critical section.
 * Exits a critical section and releases the mutex.
 * Must always be paired with eaPort_Mutex_Enter().
 * @param[in] mutex Mutex handle (must be the same one passed to eaPort_Mutex_Enter()).
 * @note Always call this to release the mutex.
 * @warning Always pair with eaPort_Mutex_Enter(); mismatched calls cause errors.
 * @see eaPort_Mutex_Enter
 */
void eaPort_Mutex_Exit(eaPort_mutex_t mutex);


/*===========================================================================*/
/* QUEUE MANAGEMENT                                                          */
/*===========================================================================*/

/**
 * @brief Creates a new queue.
 * * @param queue_length The maximum number of items the queue can hold.
 * @param item_size The size (in bytes) of each item in the queue.
 * @return eaPort_queue_t Handle to the created queue, or NULL if failed.
 */
eaPort_queue_t eaPort_Queue_Create(uint32_t queue_length, uint32_t item_size);

/**
 * @brief Deletes a queue and frees memory.
 * @param queue Handle of the queue to delete.
 */
void eaPort_Queue_Delete(eaPort_queue_t queue);

/**
 * @brief Sends an item to the queue (Copy by value).
 * * @param queue The queue handle.
 * @param item Pointer to the item to copy into the queue.
 * @param wait_ms Time to wait in milliseconds if queue is full. 
 * Use eaPort_WAIT_FOREVER or eaPort_NO_WAIT.
 * @return eaPort_status_t eaPort_STATUS_OK on success, eaPort_STATUS_ERROR on timeout/fail.
 */
eaPort_status_t eaPort_Queue_Send(eaPort_queue_t queue, const void *item, uint32_t wait_ms);

/**
 * @brief Receives an item from the queue.
 * * @param queue The queue handle.
 * @param buffer Pointer to memory where the item will be copied.
 * @param wait_ms Time to wait in milliseconds if queue is empty.
 * @return eaPort_status_t eaPort_STATUS_OK on success, eaPort_STATUS_ERROR on timeout/fail.
 */
eaPort_status_t eaPort_Queue_Receive(eaPort_queue_t queue, void *buffer, uint32_t wait_ms);

/**
 * @brief Returns the number of items currently stored in the queue.
 */
uint32_t eaPort_Queue_Messages_Waiting(eaPort_queue_t queue);


/*===========================================================================*/
/* TASK LIFECYCLE MANAGEMENT - FreeRTOS Abstraction                          */
/*===========================================================================*/

/**
 * @brief Creates and starts a task with optional core affinity.
 * Portable wrapper around FreeRTOS task creation. Supports core pinning
 * on multi-core systems and degrades gracefully on single-core systems.
 * @param[in] pxTaskCode     Task function pointer (eaPort_task_function_t signature).
 * @param[in] pcName         Human-readable task name (used for debugging).
 * @param[in] usStackDepth   Stack size in words (not bytes).
 * @param[in] pvParameters   Argument pointer passed to the task.
 * @param[in] uxPriority     Task priority (0 = lowest, higher = higher priority).
 * @param[out] pxCreatedTask Output parameter: handle to the created task (can be NULL).
 * @param[in] xCoreID        Core ID to pin the task to (0, 1, or eaPort_NO_AFFINITY).
 *                          On single-core systems, this parameter is ignored.
 * @return eaPort_status_t eaPort_STATUS_OK on success, eaPort_STATUS_ERROR on failure
 *                  (stack allocation failed, invalid priority, etc.).
 * @note Task starts immediately after creation (in Ready state).
 * @note Stack depth is in 32-bit words, not bytes.
 */
eaPort_status_t eaPort_Task_Create_Pinned_to_Core(
    eaPort_task_function_t pxTaskCode,
    const char *pcName,
    uint32_t usStackDepth,
    void *pvParameters,
    uint32_t uxPriority,
    eaPort_task_t *pxCreatedTask,
    int32_t xCoreID
);


/**
 * @brief Deletes a task and frees its resources.
 * Portable wrapper around vTaskDelete(). Removes the task from the scheduler
 * and frees its resources.
 * @param[in] xTaskToDelete Handle of the task to delete.
 *                          Pass NULL to delete the calling task.
 * @note The task must not be holding critical resources at deletion.
 * @note After deletion, the task handle is no longer valid.
 */
void eaPort_Task_Delete(eaPort_task_t xTaskToDelete);


/**
 * @brief Suspends a task (prevents it from running).
 * Portable wrapper around vTaskSuspend(). A suspended task retains its state
 * but uses no CPU time.
 * @param[in] xTaskToSuspend Handle of the task to suspend.
 *                           Pass NULL to suspend the calling task.
 * @note A task can be suspended multiple times (suspend count increments).
 * @note Each suspend requires a matching resume to fully un-suspend.
 * @see eaPort_Task_Resume, eaPort_Task_Delete
 */
void eaPort_Task_Suspend(eaPort_task_t xTaskToSuspend);


/**
 * @brief Resumes a suspended task (allows it to run again).
 *
 * Portable wrapper around vTaskResume(). Undoes the effect of
 * eaPort_Task_Suspend().
 *
 * @param[in] xTaskToResume Handle of the task to resume.
 *                          Must not be NULL.
 *
 * @note Each suspend requires a matching resume.
 * @note Attempting to resume a non-suspended task has no effect.
 *
 * @see eaPort_Task_Suspend, eaPort_Task_Create_Pinned_to_Core
 */
void eaPort_Task_Resume(eaPort_task_t xTaskToResume);


/*===========================================================================*/
/* TASK STATE RETRIEVAL AND MONITORING - FreeRTOS Abstraction               */
/*===========================================================================*/

/**
 * @brief Retrieves current task states into a dynamically allocated array.
 * Captures a snapshot of all task states and metrics. The array must be freed
 * using eaPort_Free() when no longer needed.
 * @param[out] numTasks Pointer to uint32_t to receive the actual number of tasks.
 *                       Must not be NULL.
 * @param[out] totalRunTime Pointer to uint32_t to receive total accumulated run time.
 *                          Can be NULL if not needed.
 * @return Pointer to dynamically allocated eaPort_task_info_t array on success,
 *         NULL if memory allocation fails.
 * @note The caller is responsible for freeing the returned pointer using eaPort_Free().
 */
// eaPort_task_info_t* eaPort_Get_Task_Status_Array(uint32_t *numTasks, uint32_t *totalRunTime);


/**
 * @brief Returns the run-time counter value for a task.
 * @param[in] taskStatus Pointer to the eaPort_task_info_t structure. Must not be NULL.
 * @return uint32_t Run-time counter value in system ticks.
 */
uint32_t eaPort_Get_Task_Runtime_Counter(const eaPort_task_info_t* taskStatus);


/**
 * @brief Calculates the CPU time percentage used by a task.
 * @param[in] taskStatus Pointer to the eaPort_task_info_t structure. Must not be NULL.
 * @param[in] totalRunTimeDiv100 Total system run-time divided by 100.
 * @return uint32_t CPU utilization percentage (0-100 typical range).
 */
uint32_t eaPort_Get_Task_Runtime_Percentage(const eaPort_task_info_t* taskStatus, uint32_t totalRunTimeDiv100);


/**
 * @brief Returns the name of a task.
 * @param[in] taskStatus Pointer to the eaPort_task_info_t structure. Must not be NULL.
 * @return const char* Pointer to the null-terminated task name string.
 */
const char* eaPort_Get_Task_Name(const eaPort_task_info_t* taskStatus);


/**
 * @brief Returns the priority of a task.
 * @param[in] taskStatus Pointer to the eaPort_task_info_t structure. Must not be NULL.
 * @return uint32_t Current task priority (0 is lowest).
 */
uint32_t eaPort_Get_Task_Priority(const eaPort_task_info_t* taskStatus);


/**
 * @brief Returns a human-readable string representation of task state.
 * Converts the task's state enum to a descriptive string:
 * "Running", "Ready", "Blocked", "Suspended", "Deleted", "Unknown".
 * @param[in] taskStatus Pointer to the eaPort_task_info_t structure. Must not be NULL.
 * @return const char* Pointer to a static string describing the task state.
 */
const char* eaPort_Get_Task_State_str(const eaPort_task_info_t* taskStatus);


/**
 * @brief Returns a single-character representation of task state.
 * Returns a compact encoding: 'R' = Running, 'E' = Ready, 'B' = Blocked,
 * 'S' = Suspended, 'D' = Deleted, 'U' = Unknown.
 * @param[in] taskStatus Pointer to the eaPort_task_info_t structure. Must not be NULL.
 * @return char Single character representing the task state.
 */
char eaPort_Get_Task_State_char(const eaPort_task_info_t* taskStatus);


/*===========================================================================*/
/* SYSTEM METRICS AND TIMING - FreeRTOS Abstraction                          */
/*===========================================================================*/

/**
 * @brief Returns the current system tick count.
 * Provides the absolute time reference for the RTOS in tick units.
 * Wraps around after 2^32 ticks on 32-bit systems.
 * @return eaPort_tick_t Current system tick count.
 * @note Use tick differences for duration calculations.
 */
eaPort_tick_t eaPort_Get_Tick_Time(void);


/**
 * @brief Retrieves detailed information about a specific task.
 * Populates the provided eaPort_task_info_t structure with data
 */
eaPort_status_t eaPort_Get_Task_Info (eaPort_task_info_t *taskInfo, eaPort_task_t *taskHandle);


/**
 * @brief Retrieves the handle of the currently executing task.
 * @return eaPort_task_t Handle of the current task.
 */
eaPort_task_t eaPort_Get_Current_Task_Handle();


/** 
 * @brief Retrieves the handle of a task by its name.
 * @param[in] taskName Null-terminated name of the task to find.
 * @return eaPort_task_t Handle of the task if found, NULL if not found.
 */
eaPort_task_t eaPort_Get_Task_Handle_By_Name(const char* taskName);


/**
 * @brief Retrieves and formats runtime statistics for all tasks.
 * Generates a formatted string containing CPU usage statistics for each task.
 * Useful for performance analysis and debugging.
 * @param[out] pcWriteBuffer Buffer where the formatted statistics will be written.
 *                            Caller must ensure sufficient space is available.
 * @note This is a wrapper around FreeRTOS's vTaskGetRunTimeStats().
 */
//void eaPort_Get_Task_Runtime_Stats(char *pcWriteBuffer);


/**
 * @brief Retrieves and formats a list of all tasks.
 * Generates a formatted string containing task information such as name, state,
 * priority, and stack high-water mark.
 * @param[out] pcWriteBuffer Buffer where the task list will be written.
 *                            Caller must ensure sufficient space is available.
 * @note This is a wrapper around FreeRTOS's vTaskList().
 */
//void eaPort_Get_Task_List(char *pcWriteBuffer);


#ifdef __cplusplus
}
#endif

#endif /* PORT_FREERTOS_H */
