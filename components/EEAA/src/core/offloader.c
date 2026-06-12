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
} edge_offloader_state_t;

static edge_offloader_state_t g_offloader_state = {0};

static bool offloader_state_ready(void)
{
    return g_offloader_state.initialized &&
           g_offloader_state.config.enabled &&
           g_offloader_state.policy.evaluate != NULL;
}

static const char *offloader_resolve_host(edge_offloader_route_t route, const edge_task_pair_runtime_t *runtime)
{
    if (route == EDGE_OFFLOADER_ROUTE_REMOTE) {
        if (g_offloader_state.config.remote_host_label != NULL &&
            g_offloader_state.config.remote_host_label[0] != '\0') {
            return g_offloader_state.config.remote_host_label;
        }
    } else {
        if (g_offloader_state.config.local_host_label != NULL &&
            g_offloader_state.config.local_host_label[0] != '\0') {
            return g_offloader_state.config.local_host_label;
        }
    }

    return edge_task_pair_host_name(runtime);
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

void edge_offloader_init(
    const edge_offloader_config_t *config,
    const edge_offloader_policy_t *policy)
{
    memset(&g_offloader_state, 0, sizeof(g_offloader_state));
    if (config != NULL) {
        g_offloader_state.config = *config;
    }
    if (policy != NULL) {
        g_offloader_state.policy = *policy;
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
            host = offloader_resolve_host(result->route, runtime);
            break;
        case EDGE_OFFLOADER_ROUTE_LOCAL:
            exec_site = LOCAL_EXECUTION;
            host = offloader_resolve_host(result->route, runtime);
            break;
        default:
            return false;
    }

    if (host == NULL || host[0] == '\0') {
        return false;
    }

    if (!edge_task_pair_set_host_by_index(result->task_index, host)) {
        return false;
    }

    return edge_task_pair_set_exec_site_by_index(result->task_index, exec_site);
}

bool edge_offloader_run_for_task_index(int task_index)
{
    edge_offloader_candidate_t candidate = {0};
    edge_offloader_result_t result = {0};

    if (!offloader_state_ready()) {
        return false;
    }

    if (!offloader_collect_candidate_at_index(task_index, &candidate)) {
        return false;
    }

    if (!g_offloader_state.policy.evaluate(&candidate, &result)) {
        return false;
    }

    if (result.task_index < 0) {
        result.task_index = task_index;
    }

    return edge_offloader_apply_result(&result);
}

bool edge_offloader_run_once(void)
{
    edge_offloader_candidate_t candidates[CONFIG_EA_MAX_TASKS];
    size_t candidate_count = 0U;
    bool processed_any = false;

    if (!offloader_state_ready()) {
        return false;
    }

    candidate_count = edge_offloader_collect_candidates(candidates, CONFIG_EA_MAX_TASKS);
    if (candidate_count == 0U) {
        return false;
    }

    for (size_t i = 0U; i < candidate_count && i < CONFIG_EA_MAX_TASKS; ++i) {
        edge_offloader_result_t result = {0};

        if (!g_offloader_state.policy.evaluate(&candidates[i], &result)) {
            return false;
        }

        if (result.task_index < 0) {
            result.task_index = candidates[i].task_index;
        }

        if (!edge_offloader_apply_result(&result)) {
            return false;
        }

        processed_any = true;
    }

    return processed_any;
}
