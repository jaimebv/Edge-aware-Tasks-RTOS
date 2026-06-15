/**
 * @file offloader_types.h
 * @brief Offloader routing types for EA task control.
 *
 * @details
 * The offloader works on client segments only. It decides whether a client
 * payload should stay on the local server half or route to a remote host.
 */

#ifndef OFFLOADER_TYPES_H
#define OFFLOADER_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/task_manager.h"

/** @defgroup eeaa_offloader_types Offloader types
 * @brief Routing decisions, controller configuration, and candidate views.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Routing choice for a client-side EA task.
 */
typedef enum {
    EDGE_OFFLOADER_ROUTE_LOCAL = 0,
    EDGE_OFFLOADER_ROUTE_REMOTE = 1,
} edge_offloader_route_t;

/**
 * @brief Event type emitted by the offloader observability layer.
 *
 * Route events describe successful controller mutations. Failure events
 * describe controller or policy rejections before state changes are applied.
 */
typedef enum {
    EDGE_OFFLOADER_EVENT_ROUTE_LOCAL = 0,
    EDGE_OFFLOADER_EVENT_ROUTE_REMOTE = 1,
    EDGE_OFFLOADER_EVENT_POLICY_REJECTED = 2,
    EDGE_OFFLOADER_EVENT_INVALID_VECTOR = 3,
    EDGE_OFFLOADER_EVENT_MUTATION_FAILED = 4,
} edge_offloader_event_type_t;

/**
 * @brief Controller execution mode.
 */
typedef enum {
    EDGE_OFFLOADER_MODE_PER_TASK = 0,
    EDGE_OFFLOADER_MODE_BATCH = 1,
} edge_offloader_mode_t;

/**
 * @brief Scheduler policy model used by the offloader policy layer.
 *
 * FP is the default FreeRTOS-aligned fixed-priority mode. RM is layered on
 * top of fixed-priority semantics. EDF is reserved for custom or future
 * policy modules and is not assumed to be built into the scheduler itself.
 */
typedef enum {
    EDGE_OFFLOADER_SCHEDULER_FP = 0,
    EDGE_OFFLOADER_SCHEDULER_RM = 1,
    EDGE_OFFLOADER_SCHEDULER_EDF = 2,
    EDGE_OFFLOADER_SCHEDULER_CUSTOM = 3,
} edge_offloader_scheduler_policy_t;

/**
 * @brief Static controller configuration.
 *
 * The labels are used when applying the route decision back into the task
 * manager. When the controller is enabled, both labels must be non-empty.
 * `mode` selects the per-task or batch/vector execution path.
 * `control_period_ms` is a cadence hint for the caller or scheduler; the
 * controller does not run its own timer.
 */
typedef struct {
    bool enabled;
    edge_offloader_mode_t mode;
    edge_offloader_scheduler_policy_t scheduler_policy;
    uint32_t control_period_ms;
    const char *local_host_label;
    const char *remote_host_label;
} edge_offloader_config_t;

/**
 * @brief Candidate view for a client-side EA task.
 *
 * The candidate is a snapshot plus the borrowed runtime pointer needed to
 * apply a routing decision later.
 */
typedef struct {
    int task_index;
    uint32_t pair_id;
    task_snapshot_t snapshot;
    const edge_task_pair_runtime_t *runtime;
} edge_offloader_candidate_t;

/**
 * @brief Routing result for one client-side EA task.
 */
typedef struct {
    int task_index;
    edge_offloader_route_t route;
} edge_offloader_result_t;

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* OFFLOADER_TYPES_H */
