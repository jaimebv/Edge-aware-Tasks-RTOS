/**
 * @file offloader.h
 * @brief Offloader controller API for client-side EA task routing.
 *
 * @details
 * The controller consumes task-manager snapshots for client segments, asks a
 * pure policy for a routing decision, and applies the result through task-
 * manager APIs only.
 */

#ifndef OFFLOADER_H
#define OFFLOADER_H

#include <stddef.h>
#include <stdbool.h>
#include "core/offloader_types.h"
#include "core/offloader_policy.h"

/** @defgroup eeaa_offloader Offloader controller API
 * @brief Initialization, candidate collection, decision, and application.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Event record emitted by the offloader observability layer.
 */
typedef struct {
    uint32_t sequence;
    int task_index;
    edge_offloader_route_t route;
    edge_offloader_event_type_t type;
    edge_offloader_policy_status_t policy_status;
} edge_offloader_event_t;

/**
 * @brief Aggregated offloader observability snapshot.
 *
 * The snapshot is compact enough to expose to the runtime facade and board
 * tests without leaking controller internals.
 */
typedef struct {
    size_t total_events;
    size_t route_change_events;
    size_t failure_events;
    size_t local_route_events;
    size_t remote_route_events;
    bool has_last_event;
    edge_offloader_event_t last_event;
} edge_offloader_observability_t;

/**
 * @brief Initialize the offloader controller.
 *
 * @param[in] config Static controller configuration.
 * @param[in] policy Routing policy descriptor.
 */
void edge_offloader_init(
    const edge_offloader_config_t *config,
    const edge_offloader_policy_t *policy);

/**
 * @brief Stop the offloader controller and clear its active references.
 */
void edge_offloader_shutdown(void);

/**
 * @brief Execute one offloader control cycle.
 *
 * The active controller mode determines whether the cycle uses per-task
 * routing or batch/vector routing.
 *
 * @return true when at least one candidate was processed successfully.
 */
bool edge_offloader_run_once(void);

/**
 * @brief Execute one routing decision for a known client task index.
 *
 * @param[in] task_index Monitored task index for the client segment.
 * @return true on successful evaluation and application.
 */
bool edge_offloader_run_for_task_index(int task_index);

/**
 * @brief Apply a routing result through the task manager.
 *
 * @param[in] result Routing result to apply.
 * @return true when the task manager accepted the change.
 */
bool edge_offloader_apply_result(const edge_offloader_result_t *result);

/**
 * @brief Collect client-side candidates from the current task-manager state.
 *
 * @param[out] out Candidate buffer.
 * @param[in] out_capacity Number of candidate slots in @p out.
 * @return Number of candidates written or discoverable when @p out is NULL.
 */
size_t edge_offloader_collect_candidates(
    edge_offloader_candidate_t *out,
    size_t out_capacity);

/**
 * @brief Get the active controller configuration.
 *
 * @return Pointer to the active configuration, or NULL if uninitialized.
 */
const edge_offloader_config_t *edge_offloader_current_config(void);

/**
 * @brief Get the active routing policy.
 *
 * @return Pointer to the active policy, or NULL if uninitialized.
 */
const edge_offloader_policy_t *edge_offloader_current_policy(void);

/**
 * @brief Fill a snapshot with the current offloader observability summary.
 *
 * @param[out] snapshot Observability snapshot to populate.
 * @return true when @p snapshot was populated.
 */
bool edge_offloader_observability(edge_offloader_observability_t *snapshot);

/**
 * @brief Convert an offloader event type to a static string.
 *
 * @param[in] event_type Offloader event type value.
 * @return Static string describing the event type.
 */
const char *edge_offloader_event_type_to_string(edge_offloader_event_type_t event_type);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* OFFLOADER_H */
