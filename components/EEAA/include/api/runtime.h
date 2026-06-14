/**
 * @file runtime.h
 * @brief Product-grade runtime facade for developer-facing task execution.
 *
 * @details
 * This module provides the v1 public runtime surface. It wraps the task
 * manager and offloader controller behind a small declarative API so
 * application code can configure, start, observe, and stop the runtime
 * without depending on internal ownership details.
 */

#ifndef EDGE_RUNTIME_H
#define EDGE_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/offloader.h"
#include "core/offloader_policy.h"

/** @defgroup eeaa_runtime Runtime facade API
 * @brief Developer-facing runtime lifecycle, status, and controller helpers.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Runtime lifecycle state.
 */
typedef enum {
    EDGE_RUNTIME_STATE_STOPPED = 0,
    EDGE_RUNTIME_STATE_READY = 1,
    EDGE_RUNTIME_STATE_RUNNING = 2,
} edge_runtime_state_t;

/**
 * @brief Declarative runtime configuration.
 *
 * The runtime owns a copy of the configuration passed to
 * edge_runtime_start(). The config keeps the public surface narrow while still
 * exposing the controller knobs needed by v1.
 */
typedef struct {
    edge_offloader_config_t offloader;
    const edge_offloader_policy_t *policy;
} edge_runtime_config_t;

/**
 * @brief Snapshot of the runtime state for application code and tests.
 *
 * The status object is intentionally lightweight so it can be queried on the
 * board without exposing internal runtime pointers.
 */
typedef struct {
    bool configured;
    bool running;
    bool offloader_enabled;
    edge_offloader_mode_t offloader_mode;
    uint32_t control_period_ms;
    size_t monitored_tasks;
    size_t client_candidates;
    const char *policy_name;
    const char *local_host_label;
    const char *remote_host_label;
} edge_runtime_status_t;

/**
 * @brief Initialize a runtime configuration with safe defaults.
 *
 * @param[out] config Runtime configuration to initialize.
 */
void edge_runtime_config_init(edge_runtime_config_t *config);

/**
 * @brief Start the runtime using the supplied configuration or defaults.
 *
 * Passing NULL uses the built-in defaults. A running runtime ignores repeated
 * start calls and remains active.
 *
 * @param[in] config Declarative runtime configuration, or NULL for defaults.
 * @return true when the runtime was started or was already running.
 */
bool edge_runtime_start(const edge_runtime_config_t *config);

/**
 * @brief Stop the runtime facade and shut down the controller layer.
 *
 * The task manager-owned runtime objects remain available for cleanup by the
 * caller, but the offloader controller is deactivated.
 *
 * @return true when the runtime is stopped or was already stopped.
 */
bool edge_runtime_stop(void);

/**
 * @brief Run one controller cycle through the runtime facade.
 *
 * This forwards to the offloader controller when the runtime is active.
 *
 * @return true when at least one candidate was processed successfully.
 */
bool edge_runtime_run_once(void);

/**
 * @brief Return the runtime lifecycle state.
 *
 * @return Current runtime state.
 */
edge_runtime_state_t edge_runtime_state(void);

/**
 * @brief Fill a snapshot with the current runtime state.
 *
 * @param[out] status Runtime status snapshot to populate.
 * @return true when @p status was populated.
 */
bool edge_runtime_status(edge_runtime_status_t *status);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* EDGE_RUNTIME_H */
