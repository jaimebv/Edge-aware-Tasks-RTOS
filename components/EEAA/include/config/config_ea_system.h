#ifndef CONFIG_EA_SYSTEM_H
#define CONFIG_EA_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif


/* SUPPORTED PLATFORMS 
* - ESP32
*/
#define CONFIG_EA_ESP32_BOARD  0 /**< Define ESP32 platform for conditional compilation */


/* SUPPORTED SCHEDULERS
* - FreeRTOS
*/
#define CONFIG_EA_FREERTOS_SCHEDULER  0 /**< Define FreeRTOS scheduler for conditional compilation */
#define CONFIG_EA_CUSTOM_SCHEDULER    1 /**< Define Custom scheduler for conditional compilation */

/*===========================================================================*/
/* Configuration Flags                                                      */
/*===========================================================================*/

#define CONFIG_DEBUG_FLAG   /**< Enable debug logging throughout the EEAA components */



#define CONFIG_EA_PLATFORM          CONFIG_EA_ESP32_BOARD  
#define CONFIG_EA_RTOS_SCHEDULER    CONFIG_EA_FREERTOS_SCHEDULER


#define CONFIG_EA_MAX_TASK_NAME_LEN  16
#define CONFIG_EA_MAX_TASKS          16

#ifdef __cplusplus
}
#endif
#endif /* CONFIG_EA_SYSTEM_H */