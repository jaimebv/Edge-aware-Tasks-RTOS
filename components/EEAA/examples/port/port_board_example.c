/**
 * @file port_board_example.c
 * @brief Example demonstrating the board abstraction layer (port_board).
 * 
 * This example shows how to use the board-agnostic API to retrieve
 * CPU information, cycle counts, and core IDs. 
 * 
 * The port_board layer abstracts hardware-specific features like the 
 * Xtensa CCOUNT register on ESP32, providing a portable interface for 
 * the higher layers of the Edge-aware RTOS.
 */

#include <stdio.h>
#include <inttypes.h>
#include "port/port_board.h"
#include "port/port_rtos.h" // Needed for delay functions

/**
 * @brief Test the CPU core identification functions.
 * 
 * Demonstrates:
 * - eaPort_Get_Core_id()
 * - eaPort_Get_Num_Cores()
 */
static void test_core_identification(void)
{
    printf("\n=== Test: Core Identification ===\n");
    
    uint8_t current_core = eaPort_Get_Core_id();
    uint8_t num_cores = eaPort_Get_Num_Cores();
    
    printf("Current Core ID: %u\n", current_core);
    printf("Total Cores: %u\n", num_cores);
    
    if (current_core < num_cores) {
        printf("[OK] Core ID is valid (within range 0-%u)\n", num_cores - 1);
    } else {
        printf("[FAIL] Core ID is invalid!\n");
    }
}

/**
 * @brief Test the CPU cycle and timing functions.
 * 
 * Demonstrates:
 * - eaPort_Get_Cpu_Freq()
 * - eaPort_Get_Cpu_Cycles_per_ms()
 */
static void test_cpu_timing(void)
{
    printf("\n=== Test: CPU Timing ===\n");
    
    uint32_t freq_mhz = eaPort_Get_Cpu_Freq();
    uint32_t cycles_per_ms = eaPort_Get_Cpu_Cycles_per_ms();
    
    printf("CPU Frequency: %" PRIu32 " MHz\n", freq_mhz);
    printf("Cycles per millisecond: %" PRIu32 "\n", cycles_per_ms);
    
    // Verify the relationship (cycles/ms = freq_MHz * 1000)
    if (cycles_per_ms == freq_mhz * 1000) {
        printf("[OK] Cycles per millisecond is consistent with frequency\n");
    } else {
        printf("[FAIL] Cycles per millisecond mismatch!\n");
    }
}

/**
 * @brief Test cycle counting and time conversion.
 * 
 * Demonstrates:
 * - eaPort_Get_Cpu_Cycles()
 * - eaPort_Cycles_to_ms()
 * - eaPort_Delay_Milliseconds() (from port_rtos)
 */
static void test_cycle_counting(void)
{
    printf("\n=== Test: Cycle Counting and Conversion ===\n");
    
    uint32_t cycles1 = eaPort_Get_Cpu_Cycles();
    printf("Cycle count at start: %" PRIu32 "\n", cycles1);
    
    // Simulate some work with a delay
    printf("Delaying for 100ms...\n");
    eaPort_Delay_Milliseconds(100);
    
    uint32_t cycles2 = eaPort_Get_Cpu_Cycles();
    printf("Cycle count after 100ms delay: %" PRIu32 "\n", cycles2);
    
    /* Calculate delta (handle 32-bit wrap-around) */
    uint32_t delta;
    if (cycles2 >= cycles1) {
        delta = cycles2 - cycles1;
    } else {
        delta = (0xFFFFFFFF - cycles1) + 1 + cycles2;
    }
    printf("Cycle delta: %" PRIu32 "\n", delta);
    
    uint32_t ms_converted = eaPort_Cycles_to_ms(delta);
    printf("Converted to milliseconds: ~%" PRIu32 " ms\n", ms_converted);
    
    // Check if the conversion is within a reasonable range (allow for RTOS jitter)
    if (ms_converted >= 95 && ms_converted <= 110) {
        printf("[OK] Cycle-to-millisecond conversion is accurate\n");
    } else {
        printf("[WARNING] Conversion seems off (expected ~100ms, got %" PRIu32 " ms)\n", ms_converted);
    }
}

/**
 * @brief Test the core affinity concepts.
 * 
 * Demonstrates how the system handles multiple cores.
 */
static void test_core_affinity(void)
{
    printf("\n=== Test: Core Affinity ===\n");
    
    uint8_t num_cores = eaPort_Get_Num_Cores();
    
    if (num_cores > 1) {
        printf("Multi-core system detected (%u cores)\n", num_cores);
        printf("Current task running on core: %u\n", eaPort_Get_Core_id());
    } else {
        printf("Single-core system detected\n");
    }
}

/**
 * @brief Main application entry point for ESP-IDF/RTOS.
 */
void app_main(void)
{
    printf("\n=============================================\n");
    printf("   port_board Abstraction Example\n");
    printf("   Edge-aware Tasks RTOS\n");
    printf("=============================================\n");
    
    // 1. Identification
    test_core_identification();
    
    // 2. Timing Configuration
    test_cpu_timing();
    
    // 3. Performance Measurement
    test_cycle_counting();
    
    // 4. Multi-core Check
    test_core_affinity();
    
    printf("\nSummary:\n");
    printf(" - Core ID: %u\n", eaPort_Get_Core_id());
    printf(" - CPU Freq: %" PRIu32 " MHz\n", eaPort_Get_Cpu_Freq());
    printf(" - Current Ticks: %" PRIu32 "\n", (uint32_t)eaPort_Get_Tick_Time());
    
    printf("\n[DONE] All board interface demonstrations completed!\n");
    printf("=============================================\n\n");
    
    // Keep the app running to observe output (on some platforms)
    while (1) {
        eaPort_Delay_Milliseconds(5000);
        printf("[HEARTBEAT] Ticks: %" PRIu32 "\n", (uint32_t)eaPort_Get_Tick_Time());
    }
}
