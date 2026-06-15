/**
 * @file offloader_policy_rm.c
 * @brief Built-in rate-monotonic routing policy for client-side EA tasks.
 */

#include "core/offloader_policy.h"

static bool offloader_policy_rm_evaluate_route(
    const edge_offloader_candidate_t *candidate,
    edge_offloader_result_t *result)
{
    uint32_t deadline_threshold = 0U;

    if (candidate == NULL || result == NULL) {
        return false;
    }

    if (!candidate->snapshot.valid || candidate->snapshot.period == 0U) {
        return false;
    }

    result->task_index = candidate->task_index;
    result->route = EDGE_OFFLOADER_ROUTE_LOCAL;

    deadline_threshold = candidate->snapshot.period;
    if (candidate->snapshot.OE2EL >= deadline_threshold) {
        result->route = EDGE_OFFLOADER_ROUTE_REMOTE;
    }

    return true;
}

static bool edge_offloader_policy_rm_evaluate(
    const edge_offloader_policy_context_t *context,
    const edge_offloader_candidate_t *candidate,
    edge_offloader_result_t *result,
    edge_offloader_policy_status_t *status)
{
    (void)context;

    if (status != NULL) {
        *status = EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT;
    }

    if (candidate == NULL || result == NULL) {
        return false;
    }

    if (!offloader_policy_rm_evaluate_route(candidate, result)) {
        if (status != NULL) {
            *status = EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT;
        }
        return false;
    }

    if (status != NULL) {
        *status = EDGE_OFFLOADER_POLICY_STATUS_OK;
    }
    return true;
}

static bool edge_offloader_policy_rm_plan(
    const edge_offloader_policy_context_t *context,
    const edge_offloader_candidate_t *candidates,
    size_t candidate_count,
    edge_offloader_result_t *results,
    size_t results_capacity,
    size_t *results_written,
    edge_offloader_policy_status_t *status)
{
    size_t i = 0U;

    (void)context;

    if (status != NULL) {
        *status = EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT;
    }

    if (candidates == NULL || results == NULL || results_written == NULL) {
        return false;
    }

    if (candidate_count == 0U) {
        if (status != NULL) {
            *status = EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT;
        }
        return false;
    }

    if (results_capacity < candidate_count) {
        if (status != NULL) {
            *status = EDGE_OFFLOADER_POLICY_STATUS_INSUFFICIENT_CAPACITY;
        }
        return false;
    }

    for (i = 0U; i < candidate_count; ++i) {
        if (!offloader_policy_rm_evaluate_route(&candidates[i], &results[i])) {
            if (status != NULL) {
                *status = EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT;
            }
            return false;
        }
    }

    *results_written = candidate_count;
    if (status != NULL) {
        *status = EDGE_OFFLOADER_POLICY_STATUS_OK;
    }
    return true;
}

static const edge_offloader_policy_t kRateMonotonicPolicy = {
    .name = "rate-monotonic",
    .scheduler_policy = EDGE_OFFLOADER_SCHEDULER_RM,
    .evaluate = edge_offloader_policy_rm_evaluate,
    .plan = edge_offloader_policy_rm_plan,
};

const edge_offloader_policy_t *edge_offloader_policy_rm(void)
{
    return &kRateMonotonicPolicy;
}
