/**
 * @file offloader_policy.h
 * @brief Policy interface for EA task routing decisions.
 */

#ifndef OFFLOADER_POLICY_H
#define OFFLOADER_POLICY_H

#include <stddef.h>
#include <stdbool.h>
#include "core/offloader_types.h"

/** @defgroup eeaa_offloader_policy Offloader policy API
 * @brief Pure routing policies used by the offloader controller.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct edge_offloader_policy edge_offloader_policy_t;

/**
 * @brief Policy execution context provided by the controller.
 *
 * The controller builds this context and passes it to the selected policy.
 * The policy owns the schedulability math and may inspect the runtime
 * configuration, the active scheduler model, and the controller mode.
 */
typedef struct {
    const edge_offloader_config_t *config;
    edge_offloader_scheduler_policy_t scheduler_policy;
    edge_offloader_mode_t mode;
} edge_offloader_policy_context_t;

/**
 * @brief Policy execution status.
 */
typedef enum {
    EDGE_OFFLOADER_POLICY_STATUS_OK = 0,
    EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT,
    EDGE_OFFLOADER_POLICY_STATUS_UNSUPPORTED_MODEL,
    EDGE_OFFLOADER_POLICY_STATUS_INSUFFICIENT_CAPACITY,
    EDGE_OFFLOADER_POLICY_STATUS_UNSAFE_PLAN,
    EDGE_OFFLOADER_POLICY_STATUS_INVALID_VECTOR,
} edge_offloader_policy_status_t;

/**
 * @brief Policy evaluation function.
 *
 * @param[in] candidate Candidate EA task view.
 * @param[out] result Routing decision for the candidate.
 * @return true on success, false on invalid inputs or policy failure.
 */
typedef bool (*edge_offloader_policy_eval_fn)(
    const edge_offloader_policy_context_t *context,
    const edge_offloader_candidate_t *candidate,
    edge_offloader_result_t *result,
    edge_offloader_policy_status_t *status);

/**
 * @brief Batch policy planning function.
 *
 * @param[in] candidates Candidate EA task views.
 * @param[in] candidate_count Number of candidates in @p candidates.
 * @param[out] results Planned routing decisions.
 * @param[in] results_capacity Number of slots available in @p results.
 * @param[out] results_written Number of routing decisions written to @p results.
 * @return true on success, false on invalid inputs or planning failure.
 */
typedef bool (*edge_offloader_policy_plan_fn)(
    const edge_offloader_policy_context_t *context,
    const edge_offloader_candidate_t *candidates,
    size_t candidate_count,
    edge_offloader_result_t *results,
    size_t results_capacity,
    size_t *results_written,
    edge_offloader_policy_status_t *status);

/**
 * @brief Policy descriptor used by the controller.
 */
struct edge_offloader_policy {
    const char *name;
    edge_offloader_scheduler_policy_t scheduler_policy;
    edge_offloader_policy_eval_fn evaluate;
    edge_offloader_policy_plan_fn plan;
};

/**
 * @brief Return the built-in fixed-priority policy.
 *
 * @return Pointer to a static policy descriptor.
 */
const edge_offloader_policy_t *edge_offloader_policy_fp(void);

/**
 * @brief Return the built-in rate-monotonic policy.
 *
 * @return Pointer to a static policy descriptor.
 */
const edge_offloader_policy_t *edge_offloader_policy_rm(void);

/**
 * @brief Return the compatibility alias for the built-in fixed-priority policy.
 *
 * @return Pointer to a static policy descriptor.
 */
const edge_offloader_policy_t *edge_offloader_policy_simple(void);

/**
 * @brief Return the default policy for the requested scheduler model.
 *
 * EDF and unknown models fall back to the fixed-priority built-in policy in
 * the current v1 implementation.
 *
 * @param[in] scheduler_policy Requested scheduler model.
 * @return Pointer to a static policy descriptor.
 */
const edge_offloader_policy_t *edge_offloader_policy_default_for_scheduler(
    edge_offloader_scheduler_policy_t scheduler_policy);

/**
 * @brief Convert a policy status to a static string.
 *
 * @param[in] status Policy status value.
 * @return Static string describing the policy outcome.
 */
const char *edge_offloader_policy_status_to_string(
    edge_offloader_policy_status_t status);

/**
 * @brief Convert a scheduler policy to a static string.
 *
 * @param[in] scheduler_policy Scheduler policy value.
 * @return Static string describing the scheduler model.
 */
const char *edge_offloader_scheduler_policy_to_string(
    edge_offloader_scheduler_policy_t scheduler_policy);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* OFFLOADER_POLICY_H */
