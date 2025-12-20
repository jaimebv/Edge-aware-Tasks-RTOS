//#include "port_board_wrap.h"
#include "port/port_board.h"
#include "port/port_rtos.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/**
 * @brief Test the CPU core identification functions
 */
static void test_core_identification(void)
{
    printf("\n=== Test: Core Identification ===\n");
    
    uint8_t current_core = eaPort_Get_Core_id();
    uint8_t num_cores = eaPort_Get_Num_Cores();
    
    printf("Current Core ID: %u\n", current_core);
    printf("Total Cores: %u\n", num_cores);
    
    if (current_core < num_cores) {
        printf("✓ Core ID is valid (within range 0-%u)\n", num_cores - 1);
    } else {
        printf("✗ Core ID is invalid!\n");
    }
}


/**
 * @brief Test the CPU cycle and timing functions
 */
static void test_cpu_timing(void)
{
    printf("\n=== Test: CPU Timing ===\n");
    
    uint32_t freq_mhz = eaPort_Get_Cpu_Freq();
    uint32_t cycles_per_ms = eaPort_Get_Cpu_Cycles_per_ms();
    
    printf("CPU Frequency: %lu MHz\n", freq_mhz);
    printf("Cycles per millisecond: %lu\n", cycles_per_ms);
    
    // Verify the relationship
    if (cycles_per_ms == freq_mhz * 1000) {
        printf("✓ Cycles per millisecond is consistent with frequency\n");
    } else {
        printf("✗ Cycles per millisecond mismatch!\n");
    }
}


/**
 * @brief Test cycle counting and time conversion
 */
static void test_cycle_counting(void)
{
    printf("\n=== Test: Cycle Counting and Conversion ===\n");
    
    uint32_t cycles1 = eaPort_Get_Cpu_Cycles();
    printf("Cycle count at start: %lu\n", cycles1);
    
    // Simulate some work with a small delay
    vTaskDelay(pdMS_TO_TICKS(10));
    
    uint32_t cycles2 = eaPort_Get_Cpu_Cycles();
    printf("Cycle count after 10ms delay: %lu\n", cycles2);
    
    uint32_t delta = (cycles2 >= cycles1) ? (cycles2 - cycles1) : 
                     (0xFFFFFFFF - cycles1) + 1 + cycles2;
    printf("Cycle delta: %lu\n", delta);
    
    uint32_t ms_converted = eaPort_Cycles_to_ms(delta);
    printf("Converted to milliseconds: ~%lu ms\n", ms_converted);
    
    if (ms_converted >= 9 && ms_converted <= 12) {
        printf("✓ Cycle-to-millisecond conversion is reasonable\n");
    } else {
        printf("⚠ Conversion seems off (expected ~10ms, got %lu ms)\n", ms_converted);
    }
}


/**
 * @brief Test the core-specific execution
 */
static void test_core_affinity(void)
{
    printf("\n=== Test: Core Affinity ===\n");
    
    uint8_t num_cores = eaPort_Get_Num_Cores();
    
    if (num_cores > 1) {
        printf("Multi-core system detected (%u cores)\n", num_cores);
        printf("Current task running on core: %u\n", eaPort_Get_Core_id());
        printf("✓ Multi-core platform available for testing\n");
    } else {
        printf("Single-core system detected\n");
        printf("✓ Single-core platform confirmed\n");
    }
}


/**
 * @brief Test cycling through available cores (for multi-core systems)
 */
static void test_multi_core_execution(void)
{
    printf("\n=== Test: Multi-Core Execution ===\n");
    
    uint8_t num_cores = eaPort_Get_Num_Cores();
    
    if (num_cores == 1) {
        printf("Single-core system: skipping multi-core tests\n");
        return;
    }
    
    printf("Testing execution on multiple cores:\n");
    for (int i = 0; i < 3; i++) {
        uint8_t core = eaPort_Get_Core_id();
        printf("  Iteration %d: Running on core %u\n", i, core);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}



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
        printf("✓ Task created successfully\n");
    } else {
        printf("✗ Failed to create task\n");
    }


    vTaskDelay(pdMS_TO_TICKS(10000));

    eaPort_Task_Delete(my_task);
    printf("✓ Task deleted successfully\n");
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
        printf("✗ Failed to create mutex\n");
        return;
    }
    printf("✓ Mutex created\n");
    
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
    printf("✓ Tasks deleted successfully\n");
    eaPort_Mutex_Destroy(g_mutex);
    printf("✓ Mutex destroyed successfully\n");
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
    printf("✓ Task deleted successfully\n");
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
    printf("✓ Task deleted successfully\n");
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
        printf("✗ Memory allocation failed\n");
        return;
    }
    
    printf("✓ Allocated %zu bytes\n", allocation_size);
    
    /* Use the buffer */
    memset(buffer, 0, allocation_size);
    strcpy((char*)buffer, "Hello from RTOS heap!");
    printf("Buffer content: %s\n", (char*)buffer);
    
    /* Free the memory */
    eaPort_Free(buffer);
    printf("✓ Memory freed\n");
}




/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n╔═══════════════════════════════════════════╗\n");
    printf("\n╔═══════════════════════════════════════════╗\n");
    printf("║  Platform Interface Test Suite             ║\n");
    printf("║  Edge-aware Tasks RTOS                     ║\n");
    printf("╚════════════════════════════════════════════╝\n");
    
    // Run all tests
    test_core_identification();
    test_cpu_timing();
    test_cycle_counting();
    test_core_affinity();
    test_multi_core_execution();
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Core ID: %u / %u\n", eaPort_Get_Core_id(), eaPort_Get_Num_Cores());
    printf("CPU Freq: %lu MHz\n", eaPort_Get_Cpu_Freq());
    printf("Cycles/ms: %lu\n", eaPort_Get_Cpu_Cycles_per_ms());
    printf("Current cycles: %lu\n", eaPort_Get_Cpu_Cycles());
    
    printf("\n✓ All platform interface tests completed!\n");

    
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
    //Keep the app running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        //printf("Heartbeat");
    }
}
