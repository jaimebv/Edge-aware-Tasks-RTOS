/**
 * @file platform_interface.c
 * @brief Interface for different platforms
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
 */

#include "config/config_ea_system.h"
#include "interfaces/platform_interface.h"


#if CONFIG_EA_PLATFORM == ESP32_PLATFORM
#include "port/port_esp32.h"
#else
#error "Unsupported platform configuration in platform_interface.c"
#endif

/*===========================================================================*/
uint8_t eaGet_Core_id(void)
{
    return eaPort_Get_Core_id();
}


uint8_t eaGet_Num_Cores(void)
{
    return eaPort_Get_Num_Cores();
}


uint32_t eaGet_Cpu_Cycles(void)
{
    return eaPort_Get_Cpu_Cycles();
}


uint32_t eaGet_Cpu_Freq(void)
{
    return eaPort_Get_Cpu_Freq();
}


uint32_t eaGet_Cpu_Cycles_per_ms(void) 
{
    return eaPort_Get_Cpu_Cycles_per_ms();
}


uint32_t eaCpu_Cycles_to_ms(uint32_t cycles)
{
    return eaPort_Cycles_to_ms(cycles);
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