/**
 * @file port_rtos.h
 * @brief RTOS-Agnostic Port Selector
 *
 * This is the main entry point for RTOS abstraction. Include this header
 * in your application instead of port-specific headers.
 *
 * Usage:
 * ======
 * Simply include this file in your code:
 *   #include "port/port_rtos.h"
 *
 * The appropriate RTOS-specific implementation is selected automatically
 * based on compilation environment or user configuration.
 *
 * Adding Support for a New RTOS:
 * ==============================
 * 1. Create port_<new_rtos>.h and port_<new_rtos>.c
 * 2. Follow the template in port_template.h
 * 3. Add a conditional block in this file to select it
 * 4. Update your build system to define the appropriate macro
 *
 * Example:
 * --------
 * // In your build system (CMakeLists.txt or preprocessor flags):
 * // For FreeRTOS:
 *   -DEAPORT_FREERTOS
 * // For RTX:
 *   -DEAPORT_RTX
 * // For custom RTOS:
 *   -DEAPORT_CUSTOM
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
 */

#ifndef PORT_RTOS_H
#define PORT_RTOS_H

/*===========================================================================*/
/* RTOS SELECTION                                                            */
/*===========================================================================*/

/*
 * Define EAPORT_FREERTOS to use FreeRTOS implementation
 * Define EAPORT_RTX to use Keil RTX implementation
 * Define EAPORT_CUSTOM to use a custom RTOS implementation
 *
 * If none are defined, default to FreeRTOS (most common)
 */

#include "config/config_ea_system.h"

#if CONFIG_EA_RTOS_SCHEDULER == CONFIG_EA_FREERTOS_SCHEDULER

    /* FreeRTOS is the default RTOS implementation */
    #include "port_rtos_freertos.h"

#elif CONFIG_EA_RTOS_SCHEDULER == CONFIG_EA_CUSTOM_SCHEDULER
    #include "port_custom.h"

#else
    #error "No RTOS port selected. Define EAPORT_FREERTOS, EAPORT_RTX, or EAPORT_CUSTOM"

#endif

#endif /* PORT_RTOS_H */
