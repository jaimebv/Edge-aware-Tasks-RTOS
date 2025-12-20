/**
 * @file port_boars.h
 * @brief Board-Agnostic Port Selector
 *
 * This is the main entry point for Platform-board abstraction. Include this header
 * in your application instead of port-specific headers.
 *
 * Usage:
 * ======
 * Simply include this file in your code:
 *   #include "port/port_board.h"
 *
 * The appropriate Platform-specific implementation is selected automatically
 * based on compilation environment or user configuration.
 * Adding Support for a New Platform:
 * =============================
 * 1. Create port_<new_platform>.h and port_<new_platform>.c
 * 2. Follow the template in port_template.h
 * 3. Add a conditional block in this file to select it
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
 */

#ifndef PORT_BOARD_H
#define PORT_BOARD_H
/*===========================================================================*/
/* PLATFORM SELECTION                                                        */

#include "config/config_ea_system.h"

#if CONFIG_EA_PLATFORM == CONFIG_EA_ESP32_BOARD

    /* FreeRTOS is the default RTOS implementation */
    #include "port_esp32.h"


#else
    #error "No board port selected."

#endif

#endif /* PORT_BOARD_H */