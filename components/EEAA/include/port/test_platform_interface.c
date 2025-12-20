/* Disabled to avoid duplicate app_main when src/main.c is present. */
#if 0

#include "platform_interface.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/**
 * @brief Test the CPU core identification functions
 */
static void test_core_identification(void)
{
    printf("\n=== Test: Core Identification ===\n");
    
    uint8_t current_core = eaGet_Core_id();
    uint8_t num_cores = eaGet_Num_Cores();
    
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
    
    uint32_t freq_mhz = eaGet_Cpu_Freq();
    uint32_t cycles_per_ms = eaGet_Cpu_Cycles_per_ms();
    
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
    
    uint32_t cycles1 = eaGet_Cpu_Cycles();
    printf("Cycle count at start: %lu\n", cycles1);
    
    // Simulate some work with a small delay
    vTaskDelay(pdMS_TO_TICKS(10));
    
    uint32_t cycles2 = eaGet_Cpu_Cycles();
    printf("Cycle count after 10ms delay: %lu\n", cycles2);
    
    uint32_t delta = (cycles2 >= cycles1) ? (cycles2 - cycles1) : 
                     (0xFFFFFFFF - cycles1) + 1 + cycles2;
    printf("Cycle delta: %lu\n", delta);
    
    uint32_t ms_converted = eaCpu_Cycles_to_ms(delta);
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
    
    uint8_t num_cores = eaGet_Num_Cores();
    
    if (num_cores > 1) {
        printf("Multi-core system detected (%u cores)\n", num_cores);
        printf("Current task running on core: %u\n", eaGet_Core_id());
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
    
    uint8_t num_cores = eaGet_Num_Cores();
    
    if (num_cores == 1) {
        printf("Single-core system: skipping multi-core tests\n");
        return;
    }
    
    printf("Testing execution on multiple cores:\n");
    for (int i = 0; i < 3; i++) {
        uint8_t core = eaGet_Core_id();
        printf("  Iteration %d: Running on core %u\n", i, core);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


/**
 * @brief Main application entry point
 */
void app_main(void)
{
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
    printf("Core ID: %u / %u\n", eaGet_Core_id(), eaGet_Num_Cores());
    printf("CPU Freq: %lu MHz\n", eaGet_Cpu_Freq());
    printf("Cycles/ms: %lu\n", eaGet_Cpu_Cycles_per_ms());
    printf("Current cycles: %lu\n", eaGet_Cpu_Cycles());
    
    printf("\n✓ All platform interface tests completed!\n");
    printf("Entering idle loop...\n\n");
    
    //Keep the app running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf("Heartbeat");
    }
}



#endif