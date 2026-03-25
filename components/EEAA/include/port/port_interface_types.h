/**
 * @file port_interface_types.h
 * @brief RTOS-Agnostic Type Definitions
 *
 * This module defines RTOS-agnostic types that abstract away platform-specific
 * details. Each RTOS port (FreeRTOS, RTX, etc.) must map these types to their
 * specific implementations.
 *
 * Design:
 * - eaPort_task_t: Abstracts task handle (opaque pointer)
 * - eaPort_mutex_t: Abstracts synchronization primitive (opaque pointer)
 * - eaPort_task_function_t: Task function signature
 * - eaPort_task_state_t: Enumeration of task states
 * - eaPort_task_info_t: Abstracted task status/information structure
 *
 * Each RTOS port implementation (port_freertos.h, port_rtx.h, etc.) will:
 * 1. Include this file first
 * 2. Define how these abstract types map to its specific types
 * 3. Implement the function signatures using the abstract types
 *
 * @author Jaime S Burbano
 * @version 1.0.0
 * @date 2025
 */

#ifndef PORT_INTERFACE_TYPES_H
#define PORT_INTERFACE_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* RTOS-AGNOSTIC TYPE DEFINITIONS                                           */
/*===========================================================================*/

/**
 * @brief Opaque handle to an RTOS task.
 * The actual type depends on the RTOS implementation.
 * Must be defined in the port-specific header (e.g., port_freertos.h).
 */
typedef void* eaPort_task_t;


/**
 * @brief Opaque handle to an RTOS synchronization primitive (mutex/semaphore).
 * The actual type depends on the RTOS implementation.
 * Must be defined in the port-specific header (e.g., port_freertos.h).
 */
typedef void* eaPort_mutex_t;


/**
 * @brief Task function signature.
 * All RTOS implementations must use this signature for task entry points.
 * @param[in] arg Argument passed to the task (RTOS-specific casting may be required).
 */
typedef void (*eaPort_task_function_t)(void* arg);


/**
 * @brief Enumeration of RTOS task states (RTOS-agnostic).
 * All RTOS ports must map their specific task states to these values.
 */
typedef enum {
    eaPort_TaskState_Running = 0,   /**< Task is currently running (executing on CPU) */
    eaPort_TaskState_Ready = 1,     /**< Task is ready to run but waiting for CPU */
    eaPort_TaskState_Blocked = 2,   /**< Task is blocked (waiting for event, I/O, etc.) */
    eaPort_TaskState_Suspended = 3, /**< Task is suspended (explicitly paused) */
    eaPort_TaskState_Deleted = 4,   /**< Task has been deleted */
    eaPort_TaskState_Unknown = 5    /**< Unknown/invalid state */
} eaPort_task_state_t;


/**
 * @brief RTOS-agnostic task information structure.
 * Provides abstracted task metadata that works across all RTOS implementations.
 * Port implementations must fill this structure from their RTOS-specific data.
 */
typedef struct {
    eaPort_task_t xTaskHandle;              /**< Task handle */
    const char* pcTaskName;                 /**< Task name string */
    uint32_t ulRunTimeCounter;              /**< Accumulated runtime in system ticks */
    uint32_t uxTaskNumber;                  /**< Task ID/number */
    eaPort_task_state_t eCurrentState;      /**< Current task state */
    uint32_t uxCurrentPriority;             /**< Task priority (0 = lowest) */
    uint32_t uxStackHighWaterMark;          /**< Stack high-water mark (bytes/words - RTOS dependent) */
} eaPort_task_info_t;


/**
 * @brief Return type for port operations.
 * Standardized return code: non-zero = success, zero = failure.
 */
typedef int32_t eaPort_status_t;


/**
 * @brief Port operation status codes (RTOS-agnostic).
 */
#define eaPort_STATUS_OK        1   /**< Operation succeeded */
#define eaPort_STATUS_ERROR     0   /**< Operation failed */


/**
 * @brief Initializer for a static/uninitialized mutex.
 * Port implementations should define this (typically NULL for dynamic allocation).
 */
#define eaPort_MUTEX_INIT NULL

/*===========================================================================*/
/* QUEUE DEFINITIONS                                                         */
/*===========================================================================*/

/**
 * @brief Abstract handle for a Message Queue.
 */
typedef void* eaPort_queue_t;

/**
 * @brief Timeout constants for Queue operations
 */
#define eaPort_WAIT_FOREVER     0xFFFFFFFF  /**< Block indefinitely until success */
#define eaPort_NO_WAIT          0           /**< Do not block, return immediately */


/*===========================================================================*/
/* PLATFORM-SPECIFIC DEFINITIONS                                            */
/*===========================================================================*/

/**
 * @brief Core ID for no affinity (task not pinned to a specific core).
 * Used when creating tasks on systems without core affinity requirements.
 * Port implementations that don't support core affinity should ignore this.
 */
#define eaPort_NO_AFFINITY (255)


/**
 * @brief Tick type for system time.
 * Represents the RTOS tick counter (wraps around on overflow).
 */
typedef uint32_t eaPort_tick_t;


#ifdef __cplusplus
}
#endif

#endif /* PORT_INTERFACE_TYPES_H */
