/**
 * @file offloader_policy_simple.c
 * @brief Built-in fixed-priority routing policy for client-side EA tasks.
 */

#include "core/offloader_policy.h"

static bool offloader_policy_fp_evaluate_route(
    const edge_offloader_candidate_t *candidate,
    edge_offloader_result_t *result)
{
    uint32_t remote_threshold = 0U;

    if (candidate == NULL || result == NULL) {
        return false;
    }

    if (!candidate->snapshot.valid || candidate->snapshot.WCET == 0U) {
        return false;
    }

    result->task_index = candidate->task_index;
    result->route = EDGE_OFFLOADER_ROUTE_LOCAL;

    remote_threshold = candidate->snapshot.WCET + (candidate->snapshot.WCET / 4U);
    if (candidate->snapshot.OE2EL >= remote_threshold) {
        result->route = EDGE_OFFLOADER_ROUTE_REMOTE;
    }

    return true;
}

static bool edge_offloader_policy_fp_evaluate(
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

    if (!offloader_policy_fp_evaluate_route(candidate, result)) {
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

static bool edge_offloader_policy_fp_plan(
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
        if (!offloader_policy_fp_evaluate_route(&candidates[i], &results[i])) {
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

static const edge_offloader_policy_t kFixedPriorityPolicy = {
    .name = "fixed-priority",
    .scheduler_policy = EDGE_OFFLOADER_SCHEDULER_FP,
    .evaluate = edge_offloader_policy_fp_evaluate,
    .plan = edge_offloader_policy_fp_plan,
};

const edge_offloader_policy_t *edge_offloader_policy_fp(void)
{
    return &kFixedPriorityPolicy;
}

const edge_offloader_policy_t *edge_offloader_policy_simple(void)
{
    return edge_offloader_policy_fp();
}

const edge_offloader_policy_t *edge_offloader_policy_default_for_scheduler(
    edge_offloader_scheduler_policy_t scheduler_policy)
{
    if (scheduler_policy == EDGE_OFFLOADER_SCHEDULER_RM) {
        return edge_offloader_policy_rm();
    }

    return edge_offloader_policy_fp();
}

const char *edge_offloader_policy_status_to_string(edge_offloader_policy_status_t status)
{
    switch (status) {
        case EDGE_OFFLOADER_POLICY_STATUS_OK:
            return "ok";
        case EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT:
            return "invalid-input";
        case EDGE_OFFLOADER_POLICY_STATUS_UNSUPPORTED_MODEL:
            return "unsupported-model";
        case EDGE_OFFLOADER_POLICY_STATUS_INSUFFICIENT_CAPACITY:
            return "insufficient-capacity";
        case EDGE_OFFLOADER_POLICY_STATUS_UNSAFE_PLAN:
            return "unsafe-plan";
        case EDGE_OFFLOADER_POLICY_STATUS_INVALID_VECTOR:
            return "invalid-vector";
        default:
            return "unknown";
    }
}

const char *edge_offloader_scheduler_policy_to_string(
    edge_offloader_scheduler_policy_t scheduler_policy)
{
    switch (scheduler_policy) {
        case EDGE_OFFLOADER_SCHEDULER_FP:
            return "fixed-priority";
        case EDGE_OFFLOADER_SCHEDULER_RM:
            return "rate-monotonic";
        case EDGE_OFFLOADER_SCHEDULER_EDF:
            return "edf";
        case EDGE_OFFLOADER_SCHEDULER_CUSTOM:
            return "custom";
        default:
            return "unknown";
    }
}
