/**
 * @file runtime.c
 * @brief Product-grade runtime facade for task-manager and offloader control.
 */

#include "api/runtime.h"

#include <string.h>

#include "core/task_manager.h"

typedef struct {
    bool configured;
    bool running;
    edge_runtime_config_t config;
} edge_runtime_state_store_t;

static edge_runtime_state_store_t g_runtime_state = {0};

static bool runtime_label_valid(const char *label)
{
    return label != NULL && label[0] != '\0';
}

static void runtime_config_apply_defaults(edge_runtime_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->offloader.enabled = true;
    config->offloader.mode = EDGE_OFFLOADER_MODE_PER_TASK;
    config->offloader.control_period_ms = 100U;
    config->offloader.local_host_label = "LOCAL_RUNTIME";
    config->offloader.remote_host_label = "REMOTE_RUNTIME";
    config->policy = edge_offloader_policy_simple();
}

static bool runtime_config_is_valid(const edge_runtime_config_t *config)
{
    if (config == NULL) {
        return false;
    }

    if (!config->offloader.enabled) {
        return true;
    }

    return config->offloader.control_period_ms > 0U &&
           runtime_label_valid(config->offloader.local_host_label) &&
           runtime_label_valid(config->offloader.remote_host_label);
}

static const edge_offloader_policy_t *runtime_policy_or_default(
    const edge_runtime_config_t *config)
{
    if (config != NULL && config->policy != NULL) {
        return config->policy;
    }

    return edge_offloader_policy_simple();
}

void edge_runtime_config_init(edge_runtime_config_t *config)
{
    runtime_config_apply_defaults(config);
}

bool edge_runtime_start(const edge_runtime_config_t *config)
{
    edge_runtime_config_t local_config;
    const edge_offloader_policy_t *policy = NULL;

    if (g_runtime_state.running) {
        return true;
    }

    runtime_config_apply_defaults(&local_config);
    if (config != NULL) {
        local_config = *config;
    }

    policy = runtime_policy_or_default(&local_config);
    local_config.policy = policy;

    if (!runtime_config_is_valid(&local_config)) {
        return false;
    }

    task_manager_init();
    if (local_config.offloader.enabled) {
        edge_offloader_init(&local_config.offloader, policy);
    } else {
        edge_offloader_shutdown();
    }

    g_runtime_state.config = local_config;
    g_runtime_state.configured = true;
    g_runtime_state.running = true;
    return true;
}

bool edge_runtime_stop(void)
{
    if (!g_runtime_state.configured && !g_runtime_state.running) {
        return true;
    }

    edge_offloader_shutdown();
    g_runtime_state.running = false;
    return true;
}

bool edge_runtime_run_once(void)
{
    if (!g_runtime_state.running || !g_runtime_state.config.offloader.enabled) {
        return false;
    }

    return edge_offloader_run_once();
}

edge_runtime_state_t edge_runtime_state(void)
{
    if (g_runtime_state.running) {
        return EDGE_RUNTIME_STATE_RUNNING;
    }

    if (g_runtime_state.configured) {
        return EDGE_RUNTIME_STATE_READY;
    }

    return EDGE_RUNTIME_STATE_STOPPED;
}

bool edge_runtime_status(edge_runtime_status_t *status)
{
    edge_runtime_config_t active_config;

    if (status == NULL) {
        return false;
    }

    memset(&active_config, 0, sizeof(active_config));
    if (g_runtime_state.configured) {
        active_config = g_runtime_state.config;
    }

    status->configured = g_runtime_state.configured;
    status->running = g_runtime_state.running;
    status->offloader_enabled = active_config.offloader.enabled;
    status->offloader_mode = active_config.offloader.mode;
    status->control_period_ms = active_config.offloader.control_period_ms;
    status->monitored_tasks = get_num_monitored_tasks();
    status->client_candidates = g_runtime_state.running && active_config.offloader.enabled
        ? edge_offloader_collect_candidates(NULL, 0U)
        : 0U;
    status->policy_name = active_config.policy != NULL ? active_config.policy->name : NULL;
    status->local_host_label = active_config.offloader.local_host_label;
    status->remote_host_label = active_config.offloader.remote_host_label;
    return true;
}
