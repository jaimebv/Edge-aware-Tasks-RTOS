/**
 * @file offloader_policy_simple.c
 * @brief Conservative built-in routing policy for client-side EA tasks.
 */

#include "core/offloader_policy.h"

/**
 * @brief Decide whether a client payload should stay local or route remote.
 *
 * The baseline policy is intentionally conservative:
 * - invalid inputs default to failure
 * - valid candidates default to LOCAL
 * - REMOTE is selected only when observed OE2EL exceeds the task WCET by
 *   a clear margin and the sample is otherwise valid
 */
static bool edge_offloader_policy_simple_evaluate(
    const edge_offloader_candidate_t *candidate,
    edge_offloader_result_t *result)
{
    uint32_t remote_threshold = 0U;

    if (candidate == NULL || result == NULL) {
        return false;
    }

    result->task_index = candidate->task_index;
    result->route = EDGE_OFFLOADER_ROUTE_LOCAL;

    if (!candidate->snapshot.valid || candidate->snapshot.WCET == 0U) {
        return true;
    }

    remote_threshold = candidate->snapshot.WCET + (candidate->snapshot.WCET / 4U);
    if (candidate->snapshot.OE2EL >= remote_threshold) {
        result->route = EDGE_OFFLOADER_ROUTE_REMOTE;
    }

    return true;
}

static bool edge_offloader_policy_simple_plan(
    const edge_offloader_candidate_t *candidates,
    size_t candidate_count,
    edge_offloader_result_t *results,
    size_t results_capacity,
    size_t *results_written)
{
    size_t i = 0U;

    if (candidates == NULL || results == NULL || results_written == NULL) {
        return false;
    }

    if (candidate_count == 0U || results_capacity < candidate_count) {
        return false;
    }

    for (i = 0U; i < candidate_count; ++i) {
        if (!edge_offloader_policy_simple_evaluate(&candidates[i], &results[i])) {
            return false;
        }
    }

    *results_written = candidate_count;
    return true;
}

static const edge_offloader_policy_t kSimplePolicy = {
    .name = "simple-local-first",
    .evaluate = edge_offloader_policy_simple_evaluate,
    .plan = edge_offloader_policy_simple_plan,
};

const edge_offloader_policy_t *edge_offloader_policy_simple(void)
{
    return &kSimplePolicy;
}
