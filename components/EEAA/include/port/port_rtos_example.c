/**
 * @file port_usage_example.c
 * @brief Example Usage of RTOS-Agnostic Port Layer
 *
 * This file demonstrates how to use the RTOS abstraction layer
 * to write RTOS-independent code.
 *
 * To compile with different RTOS implementations:
 * - FreeRTOS: gcc -DEAPORT_FREERTOS port_usage_example.c
 * - RTX:      gcc -DEAPORT_RTX port_usage_example.c
 *
 * @author Jaime S Burbano
 * @date 2025
 */

#include "port/port_rtos.h"
#include <stdio.h>
#include <string.h>


/*===========================================================================*/
/* EXAMPLE 1: Simple Task Creation                                          */
/*===========================================================================*/

/**
 * @brief Example task function.
 * This task prints "Hello" every second.
 */
void example_task_simple(void* arg)
{
    const char* task_name = (const char*)arg;
    printf("\n=== Test: Simple Task Creation ===\n");
    
    while (1) {
        printf("[%s] Task running at tick %lu\n", task_name, eaPort_Get_Tick_Time());
        
        /* In real FreeRTOS code, you'd use vTaskDelay here.
         * This is just for demonstration. */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void example_1_task_creation(void)
{
    eaPort_task_t my_task;
    eaPort_status_t status;
    
    /* Create a task using the RTOS-agnostic interface */
    status = eaPort_Task_Create_Pinned_to_Core(
        example_task_simple,          /* Task function */
        "SimpleTask",                 /* Task name */
        4096,                         /* Stack size in words */
        (void*)"SimpleTask",          /* Parameters */
        2,                            /* Priority (0=lowest) */
        &my_task,                     /* Output: task handle */
        0                            /* Core affinity */
    );
    
    if (status == eaPort_STATUS_OK) {
        printf(" Task created successfully\n");
    } else {
        printf(" Failed to create task\n");
    }


    vTaskDelay(pdMS_TO_TICKS(10000));

    eaPort_Task_Delete(my_task);
    printf(" Task deleted successfully\n");
}


/*===========================================================================*/
/* EXAMPLE 2: Mutex Protection                                              */
/*===========================================================================*/

/* Shared resource protected by mutex */
static int g_shared_counter = 0;
static eaPort_mutex_t g_mutex = eaPort_MUTEX_INIT;


void example_task_with_mutex(void* arg)
{
    int task_id = (int)(intptr_t)arg;
    
    while (1) {
        /* Acquire mutex before accessing shared resource */
        eaPort_Mutex_Enter(g_mutex);
        {
            int prev = g_shared_counter;
            g_shared_counter = prev + 1;
            printf("[Task %d] Counter: %d -> %d\n", 
                   task_id, prev, g_shared_counter);
        }
        eaPort_Mutex_Exit(g_mutex);
        
        /* Simulate work */
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


void example_2_mutex_protection(void)
{
    /* Create the mutex */
    g_mutex = eaPort_Mutex_Create();
    if (g_mutex == NULL) {
        printf(" Failed to create mutex\n");
        return;
    }
    printf(" Mutex created\n");
    
    /* Create multiple tasks that access shared resource */
    eaPort_task_t task1, task2;
    eaPort_Task_Create_Pinned_to_Core(
        example_task_with_mutex, "Task1", 2048, (void*)1, 2, &task1, 1
    );
    eaPort_Task_Create_Pinned_to_Core(
        example_task_with_mutex, "Task2", 2048, (void*)2, 2, &task2, 1
    );
    
    /* Later, cleanup */
    vTaskDelay(pdMS_TO_TICKS(10000));

    eaPort_Task_Delete(task1);
    eaPort_Task_Delete(task2);
    printf(" Tasks deleted successfully\n");
    eaPort_Mutex_Destroy(g_mutex);
    printf(" Mutex destroyed successfully\n");
}


/*===========================================================================*/
/* EXAMPLE 3: Task Monitoring                                               */
/*===========================================================================*/
void example_task_info(void* arg)
{
    eaPort_task_info_t task_info_test;
    eaPort_task_t current_task = eaPort_Get_Current_Task_Handle();

    while (1) {
        eaPort_Get_Task_Info(&task_info_test, &current_task);
        printf("Current Task Name: %s\n", task_info_test.pcTaskName);
        printf("Current Task Priority: %lu\n", task_info_test.uxCurrentPriority);
        printf("Current Task State: %s\n", eaPort_Get_Task_State_str(&task_info_test));
        printf("Current Task Runtime: %lu ticks\n", task_info_test.ulRunTimeCounter);
        
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void example_3_task_monitoring(void)
{


    eaPort_task_t task3;
    eaPort_Task_Create_Pinned_to_Core(
        example_task_info, "Task3", 2048, (void*)1, 2, &task3, 0
    );
    vTaskDelay(pdMS_TO_TICKS(10000));
    eaPort_Task_Delete(task3);
    printf(" Task deleted successfully\n");
}


/*===========================================================================*/
/* EXAMPLE 4: Task Suspension/Resume                                        */
/*===========================================================================*/

eaPort_task_t g_suspended_task = NULL;


void example_task_to_suspend(void* arg)
{
    int count = 0;
    while (1) {
        printf("Task running: count = %d\n", count++);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void example_4_suspend_resume(void)
{
    /* Create task */
    eaPort_Task_Create_Pinned_to_Core(
        example_task_to_suspend,
        "SuspendableTask",
        2048,
        NULL,
        2,
        &g_suspended_task,
        1
    );
    
    /* Wait a bit, then suspend */
    printf("Task is running...\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    printf("Suspending task...\n");
    eaPort_Task_Suspend(g_suspended_task);
    printf("Task suspended (no longer running)\n");
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    printf("Resuming task...\n");
    eaPort_Task_Resume(g_suspended_task);
    printf("Task resumed (running again)\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    eaPort_Task_Delete(g_suspended_task);
    printf(" Task deleted successfully\n");
}


/*===========================================================================*/
/* EXAMPLE 5: Memory Management                                             */
/*===========================================================================*/

void example_5_memory_management(void)
{
    /* Allocate memory using RTOS heap */
    size_t allocation_size = 256;
    void* buffer = eaPort_Malloc(allocation_size);
    
    if (buffer == NULL) {
        printf(" Memory allocation failed\n");
        return;
    }
    
    printf(" Allocated %zu bytes\n", allocation_size);
    
    /* Use the buffer */
    memset(buffer, 0, allocation_size);
    strcpy((char*)buffer, "Hello from RTOS heap!");
    printf("Buffer content: %s\n", (char*)buffer);
    
    /* Free the memory */
    eaPort_Free(buffer);
    printf(" Memory freed\n");
}



/*===========================================================================*/
/* MAIN FUNCTION                                                             */
/*===========================================================================*/

int main(void)
{
    printf("=================================\n");
    printf("RTOS Abstraction Layer Examples\n");
    printf("=================================\n\n");
    
    /* Example 1: Simple task creation */
    printf("[EXAMPLE 1] Simple Task Creation\n");
    printf("-----------------------------------\n");
    example_1_task_creation();
    printf("\n");
    
    /* Example 2: Mutex protection */
    printf("[EXAMPLE 2] Mutex Protection\n");
    printf("-----------------------------------\n");
    example_2_mutex_protection();
    printf("\n");
    
    /* Example 3: Task monitoring */
    printf("[EXAMPLE 3] Task Monitoring\n");
    printf("-----------------------------------\n");
    example_3_task_monitoring();
    printf("\n");
    
    /* Example 4: Task suspension/resume */
    printf("[EXAMPLE 4] Task Suspension/Resume\n");
    printf("-----------------------------------\n");
    example_4_suspend_resume();
    printf("\n");
    
    /* Example 5: Memory management */
    printf("[EXAMPLE 5] Memory Management\n");
    printf("-----------------------------------\n");
    example_5_memory_management();
    printf("\n");
    
    printf("=================================\n");
    printf("Examples completed!\n");
    printf("=================================\n");
    
    return 0;
}

