/*
 * Runtime observability regression tests.
 *
 * This harness exercises the product-facing runtime diagnostics snapshot and
 * verifies that the offloader emits route-change and failure telemetry on
 * real hardware.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_RUNTIME_OBS_PRIORITY     2U
#define TEST_RUNTIME_OBS_CORE_ID      0U
#define TEST_RUNTIME_OBS_CLIENT_STACK 2048U
#define TEST_RUNTIME_OBS_SERVER_STACK 2048U
#define TEST_RUNTIME_OBS_PERIOD_MS    1000U
#define TEST_RUNTIME_OBS_DELAY_WEIGHT 50U
#define TEST_RUNTIME_OBS_ENERGY_WEIGHT 50U
#define TEST_RUNTIME_OBS_CLIENT_WCET  200U
#define TEST_RUNTIME_OBS_REMOTE_OE2EL 400U

static const edge_task_pair_spec_t kRuntimeObsPairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static volatile bool g_runtime_obs_client_ready;
static volatile bool g_runtime_obs_server_ready;

static bool wait_until(volatile bool *flag, uint32_t timeout_ms)
{
    const uint32_t step_ms = 10U;
    uint32_t waited = 0U;

    while (!*flag && waited < timeout_ms) {
        eaPort_Delay_Milliseconds(step_ms);
        waited += step_ms;
    }

    return *flag;
}

static void reset_ready_flags(void)
{
    g_runtime_obs_client_ready = false;
    g_runtime_obs_server_ready = false;
}

static void runtime_obs_client_task(void *pvParameters)
{
    (void)pvParameters;
    g_runtime_obs_client_ready = true;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void runtime_obs_server_task(void *pvParameters)
{
    (void)pvParameters;
    g_runtime_obs_server_ready = true;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static edge_task_creation_result_t create_runtime_obs_pair(const char *task_name)
{
    edge_task_spec_t spec;

    edge_task_spec_init(&spec);
    spec.task_name = task_name;
    spec.priority = TEST_RUNTIME_OBS_PRIORITY;
    spec.client_task_code = runtime_obs_client_task;
    spec.server_task_code = runtime_obs_server_task;
    spec.client_stack_depth = TEST_RUNTIME_OBS_CLIENT_STACK;
    spec.server_stack_depth = TEST_RUNTIME_OBS_SERVER_STACK;
    spec.core_id = TEST_RUNTIME_OBS_CORE_ID;
    spec.app_type = ENRICHED;
    spec.pair_spec = kRuntimeObsPairSpec;
    spec.period_ms = TEST_RUNTIME_OBS_PERIOD_MS;
    spec.local_host_label = "OBS_BOOTSTRAP";
    spec.delay_weight = TEST_RUNTIME_OBS_DELAY_WEIGHT;
    spec.energy_weight = TEST_RUNTIME_OBS_ENERGY_WEIGHT;
    spec.client_wcet = TEST_RUNTIME_OBS_CLIENT_WCET;
    spec.server_wcet = TEST_RUNTIME_OBS_CLIENT_WCET;

    return CreateEATaskFromSpecEx(&spec);
}

static bool rejecting_eval(
    const edge_offloader_policy_context_t *context,
    const edge_offloader_candidate_t *candidate,
    edge_offloader_result_t *result,
    edge_offloader_policy_status_t *status)
{
    (void)context;
    (void)candidate;
    (void)result;

    if (status != NULL) {
        *status = EDGE_OFFLOADER_POLICY_STATUS_UNSAFE_PLAN;
    }

    return false;
}

static bool rejecting_plan(
    const edge_offloader_policy_context_t *context,
    const edge_offloader_candidate_t *candidates,
    size_t candidate_count,
    edge_offloader_result_t *results,
    size_t results_capacity,
    size_t *results_written,
    edge_offloader_policy_status_t *status)
{
    (void)context;
    (void)candidates;
    (void)candidate_count;
    (void)results;
    (void)results_capacity;
    (void)results_written;

    if (status != NULL) {
        *status = EDGE_OFFLOADER_POLICY_STATUS_UNSAFE_PLAN;
    }

    return false;
}

static const edge_offloader_policy_t kRejectingPolicy = {
    .name = "observability-rejecting",
    .scheduler_policy = EDGE_OFFLOADER_SCHEDULER_CUSTOM,
    .evaluate = rejecting_eval,
    .plan = rejecting_plan,
};

static void test_runtime_observability_stopped_snapshot(void)
{
    edge_runtime_diagnostics_t diagnostics = {0};

    edge_runtime_stop();
    expect_true("diag null safe", edge_runtime_diagnostics(NULL) == false, "diagnostics query should reject NULL");
    expect_true("diag available", edge_runtime_diagnostics(&diagnostics), "diagnostics query failed");
    expect_true("diag stopped running false", diagnostics.status.running == false, "stopped runtime should not report running");
    expect_true("diag stopped events zero", diagnostics.total_events == 0U && diagnostics.failure_events == 0U,
                "stopped diagnostics should start empty");
    expect_true("diag stopped no last event", diagnostics.has_last_event == false, "stopped diagnostics should not have a last event");

    pass("runtime observability stopped snapshot");
}

static void test_runtime_observability_reports_route_event(void)
{
    const size_t baseline = get_num_monitored_tasks();
    edge_runtime_config_t config;
    edge_runtime_diagnostics_t diagnostics = {0};
    edge_task_creation_result_t result = {0};
    const edge_task_pair_runtime_t *runtime = NULL;
    const char *host = NULL;

    edge_runtime_config_init(&config);
    config.offloader.mode = EDGE_OFFLOADER_MODE_PER_TASK;
    config.offloader.scheduler_policy = EDGE_OFFLOADER_SCHEDULER_FP;
    config.offloader.control_period_ms = 100U;
    config.offloader.local_host_label = "OBS_LOCAL";
    config.offloader.remote_host_label = "OBS_REMOTE";

    reset_ready_flags();
    expect_true("diag route runtime start", edge_runtime_start(&config), "runtime start failed");
    result = create_runtime_obs_pair("OBS_ROUTE");
    expect_true("diag route pair create", result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("diag route client ready", wait_until(&g_runtime_obs_client_ready, 5000U), "client task did not start");
    expect_true("diag route server ready", wait_until(&g_runtime_obs_server_ready, 5000U), "server task did not start");
    update_task_metrics_OE2EL_by_index(result.task_index, TEST_RUNTIME_OBS_REMOTE_OE2EL);
    expect_true("diag route run once", edge_runtime_run_once(), "runtime route cycle failed");
    expect_true("diag route status", edge_runtime_status(&diagnostics.status), "runtime status unavailable");
    expect_true("diag route candidate count", diagnostics.status.client_candidates == 1U, "candidate count mismatch");
    expect_true("diag route diagnostics", edge_runtime_diagnostics(&diagnostics), "diagnostics unavailable");
    expect_true("diag route total events", diagnostics.total_events == 1U, "route event count mismatch");
    expect_true("diag route event type", diagnostics.has_last_event &&
                diagnostics.last_event.type == EDGE_OFFLOADER_EVENT_ROUTE_REMOTE,
                "last event should be a remote route");
    expect_true("diag route event status", diagnostics.last_event.policy_status == EDGE_OFFLOADER_POLICY_STATUS_OK,
                "route event status should be ok");
    expect_true("diag route remote count", diagnostics.remote_route_events == 1U && diagnostics.local_route_events == 0U,
                "route counters mismatch");

    runtime = edge_task_pair_runtime_by_task_index(result.task_index);
    expect_true("diag route runtime present", runtime != NULL, "runtime lookup failed");
    host = edge_task_pair_local_host_label(runtime);
    expect_true("diag route host", host != NULL && strcmp(host, "OBS_REMOTE") == 0, "route should update host label");
    expect_true("diag route cleanup", edge_task_pair_destroy_by_task_index(result.task_index, EDGE_TASK_CLEANUP_PAIR) == 1,
                "cleanup failed");
    expect_true("diag route baseline restored", get_num_monitored_tasks() == baseline, "monitor baseline mismatch");
    edge_runtime_stop();

    pass("runtime observability route event");
}

static void test_runtime_observability_reports_failure_event(void)
{
    edge_runtime_config_t config;
    edge_runtime_diagnostics_t diagnostics = {0};
    edge_task_creation_result_t result = {0};

    edge_runtime_config_init(&config);
    config.offloader.mode = EDGE_OFFLOADER_MODE_PER_TASK;
    config.offloader.scheduler_policy = EDGE_OFFLOADER_SCHEDULER_CUSTOM;
    config.offloader.control_period_ms = 100U;
    config.offloader.local_host_label = "OBS_LOCAL";
    config.offloader.remote_host_label = "OBS_REMOTE";
    edge_runtime_config_set_policy(&config, &kRejectingPolicy);

    reset_ready_flags();
    expect_true("diag fail runtime start", edge_runtime_start(&config), "runtime start failed");
    result = create_runtime_obs_pair("OBS_FAIL");
    expect_true("diag fail pair create", result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("diag fail client ready", wait_until(&g_runtime_obs_client_ready, 5000U), "client task did not start");
    expect_true("diag fail server ready", wait_until(&g_runtime_obs_server_ready, 5000U), "server task did not start");
    expect_true("diag fail run once", edge_runtime_run_once() == false, "rejecting policy should fail the controller");
    expect_true("diag fail diagnostics", edge_runtime_diagnostics(&diagnostics), "diagnostics unavailable");
    expect_true("diag fail total events", diagnostics.total_events == 1U, "failure event count mismatch");
    expect_true("diag fail event type", diagnostics.has_last_event &&
                diagnostics.last_event.type == EDGE_OFFLOADER_EVENT_POLICY_REJECTED,
                "last event should be a policy rejection");
    expect_true("diag fail event status", diagnostics.last_event.policy_status == EDGE_OFFLOADER_POLICY_STATUS_UNSAFE_PLAN,
                "failure event status mismatch");
    expect_true("diag fail counters", diagnostics.route_change_events == 0U && diagnostics.failure_events == 1U,
                "failure counters mismatch");
    expect_true("diag fail host unchanged",
                strcmp(edge_task_pair_local_host_label(edge_task_pair_runtime_by_task_index(result.task_index)), "OBS_BOOTSTRAP") == 0,
                "rejection should leave the host unchanged");
    expect_true("diag fail cleanup", edge_task_pair_destroy_by_task_index(result.task_index, EDGE_TASK_CLEANUP_PAIR) == 1,
                "cleanup failed");
    edge_runtime_stop();

    pass("runtime observability failure event");
}

void test_runtime_observability_run(void)
{
    printf("=== Runtime observability test harness ===\n");
    test_helpers_reset_counts();

    test_runtime_observability_stopped_snapshot();
    test_runtime_observability_reports_route_event();
    test_runtime_observability_reports_failure_event();

    printf("=== Runtime observability tests done: passes=%u fails=%u ===\n",
           (unsigned)test_helpers_pass_count(),
           (unsigned)test_helpers_fail_count());
}
