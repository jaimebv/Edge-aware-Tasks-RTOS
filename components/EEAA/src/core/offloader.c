/**
 * @file offloader.c
 * @brief Client-side EA task routing controller.
 */

#include "core/offloader.h"

#include <string.h>

typedef struct {
    bool initialized;
    edge_offloader_config_t config;
    edge_offloader_policy_t policy;
    size_t total_events;
    size_t route_change_events;
    size_t failure_events;
    size_t local_route_events;
    size_t remote_route_events;
    bool has_last_event;
    edge_offloader_event_t last_event;
} edge_offloader_state_t;

static edge_offloader_state_t g_offloader_state = {0};

static bool offloader_label_valid(const char *label)
{
    return label != NULL && label[0] != '\0';
}

static bool offloader_config_ready(void)
{
    return g_offloader_state.config.enabled &&
           g_offloader_state.config.control_period_ms > 0U &&
           offloader_label_valid(g_offloader_state.config.local_host_label) &&
           offloader_label_valid(g_offloader_state.config.remote_host_label);
}

static bool offloader_state_ready(void)
{
    return g_offloader_state.initialized &&
           offloader_config_ready() &&
           g_offloader_state.policy.evaluate != NULL;
}

static edge_offloader_policy_context_t offloader_policy_context(void)
{
    edge_offloader_policy_context_t context = {0};

    context.config = &g_offloader_state.config;
    context.scheduler_policy = g_offloader_state.config.scheduler_policy;
    context.mode = g_offloader_state.config.mode;
    return context;
}

static bool offloader_batch_mode_enabled(void)
{
    return g_offloader_state.config.mode == EDGE_OFFLOADER_MODE_BATCH;
}

static const char *offloader_resolve_host(edge_offloader_route_t route)
{
    if (route == EDGE_OFFLOADER_ROUTE_REMOTE) {
        return g_offloader_state.config.remote_host_label;
    }

    return g_offloader_state.config.local_host_label;
}

static void offloader_observability_record(
    edge_offloader_event_type_t type,
    int task_index,
    edge_offloader_route_t route,
    edge_offloader_policy_status_t policy_status)
{
    edge_offloader_event_t event = {0};

    event.sequence = (uint32_t)(g_offloader_state.total_events + 1U);
    event.task_index = task_index;
    event.route = route;
    event.type = type;
    event.policy_status = policy_status;

    g_offloader_state.total_events++;
    if (type == EDGE_OFFLOADER_EVENT_ROUTE_LOCAL || type == EDGE_OFFLOADER_EVENT_ROUTE_REMOTE) {
        g_offloader_state.route_change_events++;
        if (type == EDGE_OFFLOADER_EVENT_ROUTE_LOCAL) {
            g_offloader_state.local_route_events++;
        } else {
            g_offloader_state.remote_route_events++;
        }
    } else {
        g_offloader_state.failure_events++;
    }

    g_offloader_state.last_event = event;
    g_offloader_state.has_last_event = true;
}

static bool offloader_collect_candidate_at_index(
    int task_index,
    edge_offloader_candidate_t *candidate)
{
    task_snapshot_t snapshot = {0};
    const edge_task_pair_runtime_t *runtime = NULL;

    if (candidate == NULL) {
        return false;
    }

    if (!get_task_snapshot_by_index(task_index, &snapshot) || !snapshot.valid) {
        return false;
    }

    if (!is_client_task(snapshot.name)) {
        return false;
    }

    runtime = edge_task_pair_runtime_by_task_index(task_index);
    if (runtime == NULL) {
        return false;
    }

    memset(candidate, 0, sizeof(*candidate));
    candidate->task_index = task_index;
    candidate->pair_id = edge_task_pair_id(runtime);
    candidate->snapshot = snapshot;
    candidate->runtime = runtime;
    return true;
}

static bool offloader_normalize_batch_results(
    const edge_offloader_candidate_t *candidates,
    const edge_offloader_result_t *results,
    size_t result_count,
    edge_offloader_result_t *normalized_results)
{
    size_t i = 0U;

    if (candidates == NULL || results == NULL || normalized_results == NULL) {
        return false;
    }

    for (i = 0U; i < result_count; ++i) {
        const edge_task_pair_runtime_t *runtime = NULL;
        edge_offloader_result_t normalized = results[i];

        if (normalized.task_index < 0) {
            normalized.task_index = candidates[i].task_index;
        }

        if (normalized.task_index != candidates[i].task_index) {
            return false;
        }

        switch (normalized.route) {
            case EDGE_OFFLOADER_ROUTE_LOCAL:
            case EDGE_OFFLOADER_ROUTE_REMOTE:
                break;
            default:
                return false;
        }

        runtime = edge_task_pair_runtime_by_task_index(candidates[i].task_index);
        if (runtime == NULL || edge_task_pair_id(runtime) != candidates[i].pair_id) {
            return false;
        }

        normalized_results[i] = normalized;
    }

    return true;
}

static bool offloader_apply_batch_results(
    const edge_offloader_candidate_t *candidates,
    const edge_offloader_result_t *results,
    size_t result_count)
{
    edge_offloader_result_t normalized_results[CONFIG_EA_MAX_TASKS];
    size_t i = 0U;

    if (result_count == 0U || result_count > CONFIG_EA_MAX_TASKS) {
        return false;
    }

    if (!offloader_normalize_batch_results(candidates, results, result_count, normalized_results)) {
        return false;
    }

    for (i = 0U; i < result_count; ++i) {
        if (!edge_offloader_apply_result(&normalized_results[i])) {
            return false;
        }
    }

    return true;
}

static bool offloader_result_is_valid(
    const edge_offloader_candidate_t *candidate,
    const edge_offloader_result_t *result)
{
    const edge_task_pair_runtime_t *runtime = NULL;

    if (candidate == NULL || result == NULL) {
        return false;
    }

    if (result->task_index < 0 || result->task_index != candidate->task_index) {
        return false;
    }

    switch (result->route) {
        case EDGE_OFFLOADER_ROUTE_LOCAL:
        case EDGE_OFFLOADER_ROUTE_REMOTE:
            break;
        default:
            return false;
    }

    runtime = edge_task_pair_runtime_by_task_index(candidate->task_index);
    if (runtime == NULL || edge_task_pair_id(runtime) != candidate->pair_id) {
        return false;
    }

    return true;
}

static bool offloader_run_per_task_cycle(void)
{
    edge_offloader_candidate_t candidates[CONFIG_EA_MAX_TASKS];
    size_t candidate_count = 0U;
    bool processed_any = false;
    size_t i = 0U;

    candidate_count = edge_offloader_collect_candidates(candidates, CONFIG_EA_MAX_TASKS);
    if (candidate_count == 0U) {
        return false;
    }

    for (i = 0U; i < candidate_count && i < CONFIG_EA_MAX_TASKS; ++i) {
        edge_offloader_result_t result = {0};
        edge_offloader_policy_status_t policy_status = EDGE_OFFLOADER_POLICY_STATUS_OK;
        edge_offloader_policy_context_t context = offloader_policy_context();

        if (!g_offloader_state.policy.evaluate(
                &context,
                &candidates[i],
                &result,
                &policy_status)) {
            return false;
        }

        if (!offloader_result_is_valid(&candidates[i], &result)) {
            return false;
        }

        if (!edge_offloader_apply_result(&result)) {
            return false;
        }

        processed_any = true;
    }

    return processed_any;
}

static bool offloader_run_batch_cycle(void)
{
    edge_offloader_candidate_t candidates[CONFIG_EA_MAX_TASKS];
    edge_offloader_result_t planned_results[CONFIG_EA_MAX_TASKS];
    edge_offloader_policy_context_t context = offloader_policy_context();
    size_t candidate_count = 0U;
    size_t planned_count = 0U;
    edge_offloader_policy_status_t policy_status = EDGE_OFFLOADER_POLICY_STATUS_OK;

    if (g_offloader_state.policy.plan == NULL) {
        return false;
    }

    candidate_count = edge_offloader_collect_candidates(candidates, CONFIG_EA_MAX_TASKS);
    if (candidate_count == 0U) {
        return false;
    }

    if (!g_offloader_state.policy.plan(
            &context,
            candidates,
            candidate_count,
            planned_results,
            candidate_count,
            &planned_count,
            &policy_status)) {
        return false;
    }

    if (planned_count != candidate_count) {
        return false;
    }

    return offloader_apply_batch_results(candidates, planned_results, planned_count);
}

void edge_offloader_init(
    const edge_offloader_config_t *config,
    const edge_offloader_policy_t *policy)
{
    const edge_offloader_policy_t *resolved_policy = NULL;

    memset(&g_offloader_state, 0, sizeof(g_offloader_state));
    if (config != NULL) {
        g_offloader_state.config = *config;
    }

    resolved_policy = policy != NULL
        ? policy
        : edge_offloader_policy_default_for_scheduler(g_offloader_state.config.scheduler_policy);
    if (resolved_policy != NULL) {
        g_offloader_state.policy = *resolved_policy;
    }
    g_offloader_state.initialized = true;
}

void edge_offloader_shutdown(void)
{
    memset(&g_offloader_state, 0, sizeof(g_offloader_state));
}

const edge_offloader_config_t *edge_offloader_current_config(void)
{
    return g_offloader_state.initialized ? &g_offloader_state.config : NULL;
}

const edge_offloader_policy_t *edge_offloader_current_policy(void)
{
    return g_offloader_state.initialized ? &g_offloader_state.policy : NULL;
}

bool edge_offloader_observability(edge_offloader_observability_t *snapshot)
{
    if (snapshot == NULL || !g_offloader_state.initialized) {
        return false;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->total_events = g_offloader_state.total_events;
    snapshot->route_change_events = g_offloader_state.route_change_events;
    snapshot->failure_events = g_offloader_state.failure_events;
    snapshot->local_route_events = g_offloader_state.local_route_events;
    snapshot->remote_route_events = g_offloader_state.remote_route_events;
    snapshot->has_last_event = g_offloader_state.has_last_event;
    if (snapshot->has_last_event) {
        snapshot->last_event = g_offloader_state.last_event;
    }

    return true;
}

const char *edge_offloader_event_type_to_string(edge_offloader_event_type_t event_type)
{
    switch (event_type) {
        case EDGE_OFFLOADER_EVENT_ROUTE_LOCAL:
            return "route-local";
        case EDGE_OFFLOADER_EVENT_ROUTE_REMOTE:
            return "route-remote";
        case EDGE_OFFLOADER_EVENT_POLICY_REJECTED:
            return "policy-rejected";
        case EDGE_OFFLOADER_EVENT_INVALID_VECTOR:
            return "invalid-vector";
        case EDGE_OFFLOADER_EVENT_MUTATION_FAILED:
            return "mutation-failed";
        default:
            return "unknown";
    }
}

size_t edge_offloader_collect_candidates(
    edge_offloader_candidate_t *out,
    size_t out_capacity)
{
    size_t collected = 0U;

    for (int task_index = 0; task_index < (int)CONFIG_EA_MAX_TASKS; ++task_index) {
        edge_offloader_candidate_t candidate = {0};

        if (!offloader_collect_candidate_at_index(task_index, &candidate)) {
            continue;
        }

        if (out != NULL && collected < out_capacity) {
            out[collected] = candidate;
        }
        ++collected;
    }

    return collected;
}

bool edge_offloader_apply_result(const edge_offloader_result_t *result)
{
    const edge_task_pair_runtime_t *runtime = NULL;
    const char *host = NULL;
    edge_task_execution_site_t exec_site = LOCAL_EXECUTION;

    if (!offloader_state_ready() || result == NULL) {
        return false;
    }

    runtime = edge_task_pair_runtime_by_task_index(result->task_index);
    if (runtime == NULL) {
        return false;
    }

    switch (result->route) {
        case EDGE_OFFLOADER_ROUTE_REMOTE:
            exec_site = REMOTE_EXECUTION;
            host = offloader_resolve_host(result->route);
            break;
        case EDGE_OFFLOADER_ROUTE_LOCAL:
            exec_site = LOCAL_EXECUTION;
            host = offloader_resolve_host(result->route);
            break;
        default:
            return false;
    }

    if (host == NULL || host[0] == '\0') {
        return false;
    }

    if (!edge_task_pair_set_local_host_label_by_index(result->task_index, host)) {
        return false;
    }

    return edge_task_pair_set_exec_site_by_index(result->task_index, exec_site);
}

bool edge_offloader_run_for_task_index(int task_index)
{
    edge_offloader_candidate_t candidate = {0};
    edge_offloader_result_t result = {0};
    edge_offloader_policy_status_t policy_status = EDGE_OFFLOADER_POLICY_STATUS_OK;
    edge_offloader_policy_context_t context = offloader_policy_context();

    if (!offloader_state_ready()) {
        return false;
    }

    if (!offloader_collect_candidate_at_index(task_index, &candidate)) {
        return false;
    }

    if (!g_offloader_state.policy.evaluate(&context, &candidate, &result, &policy_status)) {
        return false;
    }

    if (!offloader_result_is_valid(&candidate, &result)) {
        return false;
    }

    return edge_offloader_apply_result(&result);
}

bool edge_offloader_run_once(void)
{
    if (!offloader_state_ready()) {
        return false;
    }

    if (offloader_batch_mode_enabled()) {
        return offloader_run_batch_cycle();
    }

    return offloader_run_per_task_cycle();
}
