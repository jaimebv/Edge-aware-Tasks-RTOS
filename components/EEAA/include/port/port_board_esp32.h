/**
 * @file port_board_esp32.h
 * @brief ESP32 Platform Abstraction Layer Implementation
 *
 * This module provides ESP32-specific (Xtensa) implementations and interfaces
 * for hardware-level operations that are specific to the ESP32 platform.
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
 * Platform: ESP32 (Xtensa, dual-core)
 */

#ifndef PORT_BOARD_ESP32_H
#define PORT_BOARD_ESP32_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* CPU AND CORE IDENTIFICATION - ESP32 Specific                              */
/*===========================================================================*/

/**
 * @brief Returns the ID of the currently executing core/CPU.
 *
 * On ESP32 (dual-core), returns 0 or 1 depending on which core is executing
 * this code.
 *
 * @return uint8_t Core/CPU index (0 or 1 on ESP32).
 *
 * @note Safe to call from tasks.
 * @note Result is only valid at the instant of the call; avoid storing.
 *
 * @see eaPort_Get_Num_Cores
 */
uint8_t eaPort_Get_Core_id(void);


/**
 * @brief Returns the total number of cores in the system.
 *
 * On ESP32, always returns 2 (dual-core).
 *
 * @return uint8_t Number of cores (2 for ESP32).
 *
 * @note This is a compile-time constant; calling it repeatedly is safe.
 *
 * @see eaPort_Get_Core_id
 */
uint8_t eaPort_Get_Num_Cores(void);


/*===========================================================================*/
/* CPU CYCLE AND TIMING - ESP32 Specific (Xtensa)                            */
/*===========================================================================*/

/**
 * @brief Returns the current CPU cycle count.
 *
 * Reads the Xtensa CCOUNT register (32-bit cycle counter).
 * Provides high-resolution timing for performance measurement and profiling.
 *
 * @return uint32_t Current CPU cycle count (32-bit). Wraps on overflow.
 *
 * @note On ESP32 at 160 MHz, wraps every ~26.8 seconds.
 * @note Use cycle differences, not absolute values, for duration calculations.
 * @note Use eaPort_Cycles_Delta() for wrap-aware calculations.
 *
 * @warning Wraps around; handle overflow in calculations.
 *
 * @see eaPort_Cycles_to_ms, eaPort_Cycles_Delta
 */
uint32_t eaPort_Get_Cpu_Cycles(void);


/**
 * @brief Returns the CPU frequency in MHz.
 *
 * Returns the configured ESP32 clock frequency in MHz.
 * Typically 160 MHz or 240 MHz depending on sdkconfig.
 *
 * @return uint32_t CPU frequency in MHz.
 *
 * @note This is a compile-time constant; calling it repeatedly is safe.
 * @note Verify actual frequency in ESP-IDF sdkconfig: CONFIG_DEFAULT_CPU_FREQ_MHZ
 *
 * @see eaPort_Get_Cpu_Cycles_per_ms, eaPort_Cycles_to_ms
 */
uint32_t eaPort_Get_Cpu_Freq(void);


/**
 * @brief Returns the CPU frequency in cycles per millisecond.
 *
 * Convenience function: eaPort_Get_Cpu_Freq() * 1000.
 * Pre-computed constant to avoid repeated multiplication.
 *
 * @return uint32_t CPU cycles per millisecond.
 *
 * @note For ESP32 at 160 MHz: returns 160,000 cycles/ms.
 * @note For ESP32 at 240 MHz: returns 240,000 cycles/ms.
 *
 * @see eaPort_Get_Cpu_Freq
 */
uint32_t eaPort_Get_Cpu_Cycles_per_ms(void);


/**
 * @brief Converts CPU cycles to milliseconds.
 *
 * Converts a cycle count to milliseconds using the platform's CPU frequency.
 *
 * @param[in] cycles CPU cycle count.
 *
 * @return uint32_t Equivalent time in milliseconds (integer division).
 *
 * @note Result is truncated; precision may be lost.
 * @note For wrap-aware calculations, use eaPort_Cycles_Delta() first.
 * @note On ESP32: ms = cycles / 160,000 (at 160 MHz)
 *
 * @see eaPort_Get_Cpu_Cycles, eaPort_Cycles_Delta
 */
uint32_t eaPort_Cycles_to_ms(uint32_t cycles);


#ifdef __cplusplus
}
#endif

#endif /* PORT_BOARD_ESP32_H */
