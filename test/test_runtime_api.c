/*
 * Runtime facade regression tests.
 *
 * Phase 2 introduces the developer-facing runtime wrapper around the task
 * manager and offloader. These tests validate the public lifecycle API and
 * the controller forwarding path on board hardware.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_RUNTIME_PRIORITY         2U
#define TEST_RUNTIME_CORE_ID          0U
#define TEST_RUNTIME_CLIENT_STACK     2048U
#define TEST_RUNTIME_SERVER_STACK     2048U
#define TEST_RUNTIME_PERIOD_MS        1000U
#define TEST_RUNTIME_CLIENT_WCET      200U
#define TEST_RUNTIME_SERVER_WCET      200U
#define TEST_RUNTIME_DEADLINE_MS      1000U
#define TEST_RUNTIME_DELAY_WEIGHT     50U
#define TEST_RUNTIME_ENERGY_WEIGHT    50U

static const edge_task_pair_spec_t kRuntimePairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static volatile bool g_runtime_client_ready;
static volatile bool g_runtime_server_ready;

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
    g_runtime_client_ready = false;
    g_runtime_server_ready = false;
}

static void runtime_client_task(void *pvParameters)
{
    (void)pvParameters;

    g_runtime_client_ready = true;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void runtime_server_task(void *pvParameters)
{
    (void)pvParameters;

    g_runtime_server_ready = true;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static edge_task_creation_result_t create_runtime_pair(const char *task_name)
{
    edge_task_spec_t spec;

    edge_task_spec_init(&spec);
    spec.task_name = task_name;
    spec.priority = TEST_RUNTIME_PRIORITY;
    spec.client_task_code = runtime_client_task;
    spec.server_task_code = runtime_server_task;
    spec.client_stack_depth = TEST_RUNTIME_CLIENT_STACK;
    spec.server_stack_depth = TEST_RUNTIME_SERVER_STACK;
    spec.core_id = TEST_RUNTIME_CORE_ID;
    spec.app_type = ENRICHED;
    spec.default_execution_site = LOCAL_EXECUTION;
    spec.pair_spec = kRuntimePairSpec;
    spec.host_name = "LOCAL_RUNTIME";
    spec.period_ms = TEST_RUNTIME_PERIOD_MS;
    spec.deadline_ms = TEST_RUNTIME_DEADLINE_MS;
    spec.delay_weight = TEST_RUNTIME_DELAY_WEIGHT;
    spec.energy_weight = TEST_RUNTIME_ENERGY_WEIGHT;
    spec.client_wcet = TEST_RUNTIME_CLIENT_WCET;
    spec.server_wcet = TEST_RUNTIME_SERVER_WCET;

    return CreateEATaskFromSpecEx(&spec);
}

static void test_runtime_default_start_and_stop(void)
{
    edge_runtime_status_t status = {0};

    edge_runtime_stop();
    expect_true("runtime status null-safe", edge_runtime_status(NULL) == false, "status query should reject NULL");
    expect_true("runtime default start", edge_runtime_start(NULL), "default runtime start failed");
    expect_true("runtime state running", edge_runtime_state() == EDGE_RUNTIME_STATE_RUNNING, "runtime should be running");
    expect_true("runtime status available", edge_runtime_status(&status), "runtime status missing");
    expect_true("runtime status configured", status.configured, "runtime should report configured");
    expect_true("runtime status running", status.running, "runtime should report running");
    expect_true("runtime status policy name", status.policy_name != NULL && strcmp(status.policy_name, "simple-local-first") == 0,
                "runtime should expose the default policy name");
    expect_true("runtime default stop", edge_runtime_stop(), "runtime stop failed");
    expect_true("runtime state ready", edge_runtime_state() == EDGE_RUNTIME_STATE_READY, "runtime should remain ready after stop");

    pass("runtime default lifecycle");
}

static void test_runtime_invalid_config_rejected(void)
{
    edge_runtime_config_t config;

    edge_runtime_config_init(&config);
    config.offloader.enabled = true;
    config.offloader.control_period_ms = 100U;
    config.offloader.local_host_label = "";
    config.offloader.remote_host_label = "";

    expect_true("runtime invalid config rejected", edge_runtime_start(&config) == false,
                "runtime should reject empty host labels");
    expect_true("runtime state stays stopped", edge_runtime_state() == EDGE_RUNTIME_STATE_READY || edge_runtime_state() == EDGE_RUNTIME_STATE_STOPPED,
                "runtime should not become running after config rejection");

    pass("runtime invalid config guard");
}

static void test_runtime_controller_forwarding(void)
{
    const size_t baseline = get_num_monitored_tasks();
    edge_runtime_config_t config;
    edge_runtime_status_t status = {0};
    edge_task_creation_result_t result = {0};
    const edge_task_pair_runtime_t *runtime = NULL;
    const char *host = NULL;

    edge_runtime_config_init(&config);
    config.offloader.local_host_label = "RUNTIME_LOCAL";
    config.offloader.remote_host_label = "RUNTIME_REMOTE";
    config.offloader.mode = EDGE_OFFLOADER_MODE_PER_TASK;
    config.offloader.control_period_ms = 100U;

    reset_ready_flags();
    expect_true("runtime configured start", edge_runtime_start(&config), "runtime configured start failed");
    result = create_runtime_pair("RuntimeApi");
    expect_true("runtime pair create", result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("runtime status after create", edge_runtime_status(&status), "runtime status unavailable after create");
    expect_true("runtime status monitored count", status.monitored_tasks == baseline + 2U, "monitored count mismatch");
    expect_true("runtime client ready", wait_until(&g_runtime_client_ready, 5000U), "client task did not start");
    expect_true("runtime server ready", wait_until(&g_runtime_server_ready, 5000U), "server task did not start");
    expect_true("runtime status candidate count", edge_runtime_status(&status) && status.client_candidates == 1U,
                "client candidate count mismatch");
    expect_true("runtime run once", edge_runtime_run_once(), "runtime controller tick failed");

    runtime = edge_task_pair_runtime_by_task_index(result.task_index);
    expect_true("runtime forwarded host", runtime != NULL, "runtime lookup failed");
    host = edge_task_pair_host_name(runtime);
    expect_true("runtime forwarded host label", host != NULL && strcmp(host, "RUNTIME_LOCAL") == 0,
                "runtime host should be updated through the facade");
    expect_true("runtime forwarded exec site", strcmp(get_task_ex_site_by_index(result.task_index), "LOCAL_EXECUTION") == 0,
                "runtime execution site should be updated through the facade");
    expect_true("runtime cleanup", edge_task_pair_destroy_by_task_index(result.task_index, EDGE_TASK_CLEANUP_PAIR) == 1,
                "runtime cleanup failed");
    expect_true("runtime stop after forwarding", edge_runtime_stop(), "runtime stop failed");
    expect_true("runtime baseline restored", get_num_monitored_tasks() == baseline, "runtime should restore baseline");

    pass("runtime controller forwarding");
}

void test_runtime_api_run(void)
{
    printf("=== Runtime facade test harness ===\n");
    edge_runtime_stop();

    test_runtime_default_start_and_stop();
    edge_runtime_stop();

    test_runtime_invalid_config_rejected();
    edge_runtime_stop();

    test_runtime_controller_forwarding();
    edge_runtime_stop();

    printf("=== Runtime facade tests done: passes=%u fails=%u ===\n",
           (unsigned)test_helpers_pass_count(),
           (unsigned)test_helpers_fail_count());
}
