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

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* OFFLOADER_H */
