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
 * @brief Policy evaluation function.
 *
 * @param[in] candidate Candidate EA task view.
 * @param[out] result Routing decision for the candidate.
 * @return true on success, false on invalid inputs or policy failure.
 */
typedef bool (*edge_offloader_policy_eval_fn)(
    const edge_offloader_candidate_t *candidate,
    edge_offloader_result_t *result);

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
    const edge_offloader_candidate_t *candidates,
    size_t candidate_count,
    edge_offloader_result_t *results,
    size_t results_capacity,
    size_t *results_written);

/**
 * @brief Policy descriptor used by the controller.
 */
struct edge_offloader_policy {
    const char *name;
    edge_offloader_policy_eval_fn evaluate;
    edge_offloader_policy_plan_fn plan;
};

/**
 * @brief Return the built-in conservative policy.
 *
 * @return Pointer to a static policy descriptor.
 */
const edge_offloader_policy_t *edge_offloader_policy_simple(void);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* OFFLOADER_POLICY_H */
