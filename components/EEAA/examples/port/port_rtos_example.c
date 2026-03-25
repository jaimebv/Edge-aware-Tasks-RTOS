/**
 * @file port_rtos_example.c
 * @brief Example demonstrating the RTOS abstraction layer (port_rtos).
 * 
 * This example shows how to write RTOS-agnostic code using the EA-RTOS
 * porting layer. It covers:
 * - Task management (Creation, Deletion, Suspend/Resume)
 * - Synchronization (Mutexes)
 * - Communication (Queues)
 * - Memory management (Malloc/Free)
 * - System metrics (Ticks, Delays)
 * 
 * To switch between RTOS backends, update CONFIG_EA_RTOS_SCHEDULER in 
 * your configuration.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "port/port_rtos.h"

/*===========================================================================*/
/* EXAMPLE 1: Simple Task Management                                         */
/*===========================================================================*/

static void example_task_simple(void* arg)
{
    const char* task_name = (const char*)arg;
    printf("[%s] Task started at tick %" PRIu32 "\n", task_name, (uint32_t)eaPort_Get_Tick_Time());
    
    for (int i = 0; i < 5; i++) {
        printf("[%s] Iteration %d, time: %" PRIu32 " ms\n", 
               task_name, i, (uint32_t)eaPort_Get_Tick_Time()); // Note: ticks approx ms on many systems
        eaPort_Delay_Milliseconds(500);
    }
    
    printf("[%s] Task finishing...\n", task_name);
    eaPort_Task_Delete(NULL); // Delete self
}

static void run_example_1_tasks(void)
{
    printf("\n--- Example 1: Task Creation ---\n");
    eaPort_task_t my_task;
    
    eaPort_status_t status = eaPort_Task_Create_Pinned_to_Core(
        example_task_simple, "SimpleTask", 2048, (void*)"SimpleTask", 2, &my_task, eaPort_NO_AFFINITY
    );
    
    if (status == eaPort_STATUS_OK) {
        printf(" Task 'SimpleTask' created successfully.\n");
    }
    
    // Wait for task to finish its iterations
    eaPort_Delay_Milliseconds(3000);
}

/*===========================================================================*/
/* EXAMPLE 2: Mutex Protection                                              */
/*===========================================================================*/

static int g_shared_resource = 0;
static eaPort_mutex_t g_mutex = eaPort_MUTEX_INIT;

static void task_mutex_worker(void* arg)
{
    int id = (int)(intptr_t)arg;
    for (int i = 0; i < 3; i++) {
        eaPort_Mutex_Enter(g_mutex);
        {
            g_shared_resource++;
            printf("[Worker %d] Incremented counter to %d\n", id, g_shared_resource);
        }
        eaPort_Mutex_Exit(g_mutex);
        eaPort_Delay_Milliseconds(100);
    }
    eaPort_Task_Delete(NULL);
}

static void run_example_2_mutex(void)
{
    printf("\n--- Example 2: Mutex Protection ---\n");
    g_mutex = eaPort_Mutex_Create();
    
    eaPort_Task_Create_Pinned_to_Core(task_mutex_worker, "Worker1", 2048, (void*)1, 2, NULL, eaPort_NO_AFFINITY);
    eaPort_Task_Create_Pinned_to_Core(task_mutex_worker, "Worker2", 2048, (void*)2, 2, NULL, eaPort_NO_AFFINITY);
    
    eaPort_Delay_Milliseconds(1000);
    eaPort_Mutex_Destroy(g_mutex);
}

/*===========================================================================*/
/* EXAMPLE 3: Message Queues                                                 */
/*===========================================================================*/

typedef struct {
    uint32_t id;
    char message[20];
} app_msg_t;

static eaPort_queue_t g_app_queue = NULL;

static void task_producer(void* arg)
{
    app_msg_t msg;
    for (uint32_t i = 0; i < 3; i++) {
        msg.id = i;
        snprintf(msg.message, sizeof(msg.message), "Hello %" PRIu32, i);
        printf("[Producer] Sending: %s\n", msg.message);
        eaPort_Queue_Send(g_app_queue, &msg, eaPort_WAIT_FOREVER);
        eaPort_Delay_Milliseconds(200);
    }
    eaPort_Task_Delete(NULL);
}

static void task_consumer(void* arg)
{
    app_msg_t received;
    for (int i = 0; i < 3; i++) {
        if (eaPort_Queue_Receive(g_app_queue, &received, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            printf("[Consumer] Received ID %" PRIu32 ": %s\n", received.id, received.message);
        }
    }
    eaPort_Task_Delete(NULL);
}

static void run_example_3_queues(void)
{
    printf("\n--- Example 3: Message Queues ---\n");
    g_app_queue = eaPort_Queue_Create(5, sizeof(app_msg_t));
    
    eaPort_Task_Create_Pinned_to_Core(task_producer, "Prod", 2048, NULL, 2, NULL, eaPort_NO_AFFINITY);
    eaPort_Task_Create_Pinned_to_Core(task_consumer, "Cons", 2048, NULL, 2, NULL, eaPort_NO_AFFINITY);
    
    eaPort_Delay_Milliseconds(1000);
    eaPort_Queue_Delete(g_app_queue);
}

/*===========================================================================*/
/* EXAMPLE 4: Task Suspend / Resume                                          */
/*===========================================================================*/

static void task_to_be_suspended(void* arg)
{
    while (1) {
        printf(" [BusyTask] Still alive... tick %" PRIu32 "\n", (uint32_t)eaPort_Get_Tick_Time());
        eaPort_Delay_Milliseconds(500);
    }
}

static void run_example_4_suspend(void)
{
    printf("\n--- Example 4: Suspend/Resume ---\n");
    eaPort_task_t target;
    eaPort_Task_Create_Pinned_to_Core(task_to_be_suspended, "Busy", 2048, NULL, 2, &target, eaPort_NO_AFFINITY);
    
    eaPort_Delay_Milliseconds(1200);
    printf(" Suspending task...\n");
    eaPort_Task_Suspend(target);
    
    eaPort_Delay_Milliseconds(1500);
    printf(" Resuming task...\n");
    eaPort_Task_Resume(target);
    
    eaPort_Delay_Milliseconds(1000);
    eaPort_Task_Delete(target);
}

/*===========================================================================*/
/* EXAMPLE 5: Memory Management                                              */
/*===========================================================================*/

static void run_example_5_memory(void)
{
    printf("\n--- Example 5: Memory Management ---\n");
    char* dynamic_str = (char*)eaPort_Malloc(32);
    if (dynamic_str) {
        strcpy(dynamic_str, "Dynamic string from heap");
        printf(" Allocated content: %s\n", dynamic_str);
        eaPort_Free(dynamic_str);
        printf(" Memory freed.\n");
    }
}

/*===========================================================================*/
/* MAIN ENTRY POINT                                                          */
/*===========================================================================*/

void app_main(void)
{
    printf("\n=============================================\n");
    printf("   port_rtos Abstraction Example\n");
    printf("   Edge-aware Tasks RTOS\n");
    printf("=============================================\n");
    
    run_example_1_tasks();
    run_example_2_mutex();
    run_example_3_queues();
    run_example_4_suspend();
    run_example_5_memory();
    
    printf("\n[DONE] All RTOS abstraction demonstrations completed!\n");
    printf("=============================================\n\n");
    
    // Summary of system state
    uint32_t now = (uint32_t)eaPort_Get_Tick_Time();
    printf("Final System State:\n");
    printf(" - Ticks elapsed: %" PRIu32 "\n", now);
    
    while(1) {
        eaPort_Delay_Milliseconds(5000);
        printf("[HEARTBEAT] System alive. Ticks: %" PRIu32 "\n", (uint32_t)eaPort_Get_Tick_Time());
    }
}

