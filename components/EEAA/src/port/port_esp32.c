/**
 * @file port_esp32.c
 * @brief ESP32 Platform Abstraction Layer Implementation
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
*/
#include "config/config_ea_system.h"
#include "port/port_esp32.h"

#include "esp_private/esp_clk.h"
#include <xtensa/hal.h>
#include <string.h>
#include <limits.h>

/*===========================================================================*/
/* PLATFORM CONFIGURATION - ESP32 Specific                                   */
/*===========================================================================*/


#ifdef portNUM_PROCESSORS
#define ESP32_NUM_CORES              portNUM_PROCESSORS
#else
#define ESP32_NUM_CORES              2
#endif

#if CONFIG_EA_RTOS_SCHEDULER == CONFIG_EA_FREERTOS_SCHEDULER
#include "freertos/FreeRTOS.h"
#endif

/*===========================================================================*/
/* CPU AND CORE IDENTIFICATION                                               */
/*===========================================================================*/

uint8_t eaPort_Get_Core_id(void)
{
    #if CONFIG_EA_RTOS_SCHEDULER == CONFIG_EA_FREERTOS_SCHEDULER
    /* On ESP32, xPortGetCoreID() returns 0 or 1 depending on the current core.
     * This is an FreeRTOS/ESP-IDF function. */
    return (uint8_t)xPortGetCoreID();
    #endif
    return 255; /* Unknown core ID */
}


uint8_t eaPort_Get_Num_Cores(void)
{
    /* ESP32 is a dual-core processor */
    return ESP32_NUM_CORES;
}


/*===========================================================================*/
/* CPU CYCLE AND TIMING                                                      */
/*===========================================================================*/

uint32_t eaPort_Get_Cpu_Cycles(void)
{
    /* Xtensa-specific function to read the cycle counter (CCOUNT register).
     * Wraps every ~26.8 seconds at 160 MHz (at 2^32 cycles). */
    return (uint32_t)xthal_get_ccount();
}


uint32_t eaPort_Get_Cpu_Freq(void)
{
    /* Return the configured CPU frequency in MHz.
     * On ESP32, this is typically 160 or 240 MHz. */
    return (uint32_t)esp_clk_cpu_freq()/1000000;
}


uint32_t eaPort_Get_Cpu_Cycles_per_ms(void)
{
    /* Pre-computed constant: cycles per millisecond.
     * For 160 MHz: 160,000 cycles/ms
     * This avoids runtime multiplication. */
    return eaPort_Get_Cpu_Freq() * 1000UL;
}


uint32_t eaPort_Cycles_to_ms(uint32_t cycles)
{
    /* Convert CPU cycles to milliseconds.
     * Uses the CPU frequency to calculate: ms = cycles / (freq_MHz * 1000)
     * For 160 MHz: ms = cycles / 160000 */
    uint32_t freq = eaPort_Get_Cpu_Freq();
    if (freq == 0) {
        return 0;
    }
    return cycles / (freq * 1000UL);
}



uint32_t cpu_cycles_delta(uint32_t current, uint32_t previous)
{
    /* Compute wrap-aware delta between two cycle counts.
     * Handles 32-bit counter wrap-around correctly.
     * 
     * When the cycle counter wraps from 0xFFFFFFFF back to 0, we need to
     * account for this in the delta calculation.
     * 
     * Example:
     *   previous = 0xFFFFFFF0 (very close to wrap-around)
     *   current  = 0x00000010 (wrapped around)
     *   delta = (0xFFFFFFFF - 0xFFFFFFF0) + 1 + 0x00000010
     *         = 0x0000000F + 1 + 0x00000010
     *         = 0x00000020 (32 cycles elapsed)
     */
    if (current >= previous) {
        /* No wrap-around: simple subtraction */
        return current - previous;
    } else {
        /* Wrap-around detected: (0xFFFFFFFF - previous) + 1 + current
         * Which simplifies to: (UINT32_MAX - previous) + current + 1 */
        return (UINT32_MAX - previous) + current + 1U;
    }
}


uint32_t cpu_cycles_to_ms(uint32_t cycles)
{
    /* Convert cycle count to milliseconds.
     * ms = cycles / (freq_mhz * 1000)
     * Uses integer division (truncation). */
    return cycles / eaPort_Get_Cpu_Cycles_per_ms();
}