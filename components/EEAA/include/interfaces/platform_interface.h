/**
 * @file platform_interface.h
 * @brief Interface for different platforms
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
 */

#ifndef PLATFORM_INTERFACE_H
#define PLATFORM_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/*===========================================================================*/
/* CPU AND CORE IDENTIFICATION                            */
/*===========================================================================*/

/**
 * @brief Returns the ID of the currently executing core/CPU.
 * @return uint8_t Core/CPU index (starts in 0).
 * @note Result is only valid at the instant of the call; avoid storing.
 */
uint8_t eaGet_Core_id(void);


/**
 * @brief Returns the total number of cores in the system.
 * @return uint8_t Number of cores.
 * @note This is a compile-time constant; calling it repeatedly is safe.
 */
uint8_t eaGet_Num_Cores(void);


/*===========================================================================*/
/* CPU CYCLE AND TIMING                          */
/*===========================================================================*/

/**
 * @brief Returns the current CPU cycle count.
 * Provides high-resolution timing for performance measurement and profiling.
 * @return uint32_t Current CPU cycle count (32-bit). Wraps on overflow.
 * @note Use cycle differences, not absolute values, for duration calculations.
 * @note Use eaPort_Cycles_Delta() for wrap-aware calculations.
 * @warning Wraps around; handle overflow in calculations.
 */
uint32_t eaGet_Cpu_Cycles(void);


/**
 * @brief Returns the CPU frequency in MHz.
 * @return uint32_t CPU frequency in MHz.
 * @note This is a compile-time constant; calling it repeatedly is safe.
 */
uint32_t eaGet_Cpu_Freq(void);


/**
 * @brief Returns the CPU frequency in cycles per millisecond.
 * @return uint32_t CPU cycles per millisecond.
 */
uint32_t eaGet_Cpu_Cycles_per_ms(void);


/**
 * @brief Converts CPU cycles to milliseconds.
 * Converts a cycle count to milliseconds using the platform's CPU frequency.
 * @param[in] cycles CPU cycle count.
 * @return uint32_t Equivalent time in milliseconds (integer division).
 * @note Result is truncated; precision may be lost.
 * @note For wrap-aware calculations, use eaPort_Cycles_Delta() first.
 */
uint32_t eaCpu_Cycles_to_ms(uint32_t cycles);




#ifdef __cplusplus
}
#endif

 #endif /* PLATFORM_INTERFACE_H */