/*
 * Offloader policy regression tests.
 *
 * Phase 3 covers the pure routing policy.
 * Phase 4 extends the same module with controller candidate collection and
 * application of routing decisions through the task manager.
 */

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "core/offloader.h"
#include "core/offloader_policy.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_OFFLOADER_PRIORITY        2U
#define TEST_OFFLOADER_CORE_ID         0U
#define TEST_OFFLOADER_CLIENT_STACK    2048U
#define TEST_OFFLOADER_SERVER_STACK    2048U
#define TEST_OFFLOADER_PERIOD_MS       1000U
#define TEST_OFFLOADER_CLIENT_WCET     200U
#define TEST_OFFLOADER_SERVER_WCET     400U
#define TEST_OFFLOADER_MAE2EL          1000U
#define TEST_OFFLOADER_DELAY_WEIGHT    50U
#define TEST_OFFLOADER_ENERGY_WEIGHT   50U

static const edge_task_pair_spec_t kOffloaderPairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static volatile bool g_offloader_client_ready;
static volatile bool g_offloader_server_ready;

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

static void offloader_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;

    if (runtime == NULL) {
        fail("offloader client startup", "missing runtime");
        eaPort_Task_Delete(NULL);
        return;
    }

    g_offloader_client_ready = true;
    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void offloader_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;

    if (runtime == NULL) {
        fail("offloader server startup", "missing runtime");
        eaPort_Task_Delete(NULL);
        return;
    }

    g_offloader_server_ready = true;
    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static edge_task_creation_result_t create_offloader_pair(const char *base_name)
{
    return CreateEATaskPinnedToCoreEx(
        base_name,
        TEST_OFFLOADER_PRIORITY,
        offloader_client_task,
        offloader_server_task,
        TEST_OFFLOADER_CLIENT_STACK,
        TEST_OFFLOADER_SERVER_STACK,
        TEST_OFFLOADER_CORE_ID,
        ENRICHED,
        TEST_OFFLOADER_MAE2EL,
        TEST_OFFLOADER_DELAY_WEIGHT,
        TEST_OFFLOADER_ENERGY_WEIGHT,
        LOCAL_EXECUTION,
        &kOffloaderPairSpec,
        "bootstrap-host",
        TEST_OFFLOADER_PERIOD_MS,
        TEST_OFFLOADER_CLIENT_WCET,
        TEST_OFFLOADER_SERVER_WCET);
}

static void test_policy_null_inputs(void)
{
    edge_offloader_candidate_t candidate = {0};
    edge_offloader_result_t result = {0};
    const edge_offloader_policy_t *policy = edge_offloader_policy_simple();

    expect_true("policy descriptor present", policy != NULL, "policy descriptor missing");
    expect_true("policy evaluate null candidate", policy->evaluate(NULL, &result) == false, "null candidate should fail");
    expect_true("policy evaluate null result", policy->evaluate(&candidate, NULL) == false, "null result should fail");

    pass("policy null input guards");
}

static void test_policy_local_default(void)
{
    edge_offloader_candidate_t candidate = {0};
    edge_offloader_result_t result = {0};
    const edge_offloader_policy_t *policy = edge_offloader_policy_simple();

    candidate.task_index = 7;
    candidate.pair_id = 11U;
    candidate.snapshot.valid = true;
    candidate.snapshot.WCET = 400U;
    candidate.snapshot.OE2EL = 430U;
    candidate.snapshot.cpu_cycles = 900U;
    candidate.runtime = NULL;

    expect_true("policy evaluate local", policy->evaluate(&candidate, &result), "local decision should succeed");
    expect_true("policy local task index", result.task_index == 7, "task index mismatch");
    expect_true("policy local route", result.route == EDGE_OFFLOADER_ROUTE_LOCAL, "candidate should stay local");

    pass("policy local default");
}

static void test_policy_remote_threshold(void)
{
    edge_offloader_candidate_t candidate = {0};
    edge_offloader_result_t result = {0};
    const edge_offloader_policy_t *policy = edge_offloader_policy_simple();

    candidate.task_index = 12;
    candidate.pair_id = 21U;
    candidate.snapshot.valid = true;
    candidate.snapshot.WCET = 400U;
    candidate.snapshot.OE2EL = 520U;
    candidate.snapshot.cpu_cycles = 1200U;
    candidate.runtime = NULL;

    expect_true("policy evaluate remote", policy->evaluate(&candidate, &result), "remote decision should succeed");
    expect_true("policy remote task index", result.task_index == 12, "task index mismatch");
    expect_true("policy remote route", result.route == EDGE_OFFLOADER_ROUTE_REMOTE, "candidate should route remote");

    pass("policy remote threshold");
}

static void test_controller_candidate_collection_and_routing(void)
{
    const edge_offloader_config_t config = {
        .enabled = true,
        .control_period_ms = 100U,
        .local_host_label = "LOCAL_ROUTE",
        .remote_host_label = "REMOTE_ROUTE",
    };
    const edge_offloader_policy_t *policy = edge_offloader_policy_simple();
    edge_task_creation_result_t creation = {0};
    const edge_offloader_config_t *current_config = NULL;
    const edge_offloader_policy_t *current_policy = NULL;
    edge_offloader_candidate_t candidate = {0};
    edge_offloader_candidate_t candidate_buffer[2] = {0};
    const edge_task_pair_runtime_t *runtime = NULL;
    const char *host = NULL;
    int client_index = -1;
    int server_index = -1;

    g_offloader_client_ready = false;
    g_offloader_server_ready = false;

    expect_true("controller policy descriptor", policy != NULL, "policy descriptor missing");
    edge_offloader_init(&config, policy);
    current_config = edge_offloader_current_config();
    current_policy = edge_offloader_current_policy();
    expect_true("controller config active", current_config != NULL && current_config->enabled, "controller config missing");
    expect_true("controller policy active", current_policy != NULL && current_policy->evaluate != NULL, "controller policy missing");
    expect_true("controller policy name", current_policy->name != NULL && strcmp(current_policy->name, "simple-local-first") == 0, "policy name mismatch");
    expect_true("controller invalid run", edge_offloader_run_for_task_index(-1) == false, "invalid index should fail");

    creation = create_offloader_pair("OFLOW");
    expect_true("controller pair create", creation.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && creation.task_index >= 0, "offloader pair creation failed");
    client_index = creation.task_index;
    runtime = edge_task_pair_runtime_by_task_index(client_index);
    expect_true("controller runtime present", runtime != NULL, "runtime missing");
    server_index = edge_task_pair_peer_index(runtime);
    expect_true("controller server index", server_index >= 0, "server index missing");
    expect_true("controller client ready", wait_until(&g_offloader_client_ready, 5000U), "client task did not start");
    expect_true("controller server ready", wait_until(&g_offloader_server_ready, 5000U), "server task did not start");

    update_task_metrics_OE2EL_by_index(client_index, 100U);
    expect_true("controller candidate count", edge_offloader_collect_candidates(NULL, 0U) == 1U, "candidate count mismatch");
    expect_true("controller candidate buffer count", edge_offloader_collect_candidates(candidate_buffer, 2U) == 1U, "candidate buffer count mismatch");
    candidate = candidate_buffer[0];
    expect_true("controller candidate index", candidate.task_index == client_index, "candidate index mismatch");
    expect_true("controller candidate pair id", candidate.pair_id == edge_task_pair_id(runtime), "candidate pair id mismatch");
    expect_true("controller candidate runtime", candidate.runtime == runtime, "candidate runtime mismatch");
    expect_true("controller candidate snapshot valid", candidate.snapshot.valid, "candidate snapshot invalid");
    expect_true("controller candidate snapshot name", strstr(candidate.snapshot.name, "-cl-") != NULL, "candidate should be client-side");
    expect_true("controller run local", edge_offloader_run_once(), "controller local run failed");
    host = edge_task_pair_host_name(edge_task_pair_runtime_by_task_index(client_index));
    expect_true("controller local exec site", strcmp(get_task_ex_site_by_index(client_index), "LOCAL_EXECUTION") == 0, "local execution site mismatch");
    expect_true("controller local host", host != NULL && strcmp(host, config.local_host_label) == 0, "local host mismatch");

    update_task_metrics_OE2EL_by_index(client_index, 600U);
    expect_true("controller run remote", edge_offloader_run_once(), "controller remote run failed");
    host = edge_task_pair_host_name(edge_task_pair_runtime_by_task_index(client_index));
    expect_true("controller remote exec site", strcmp(get_task_ex_site_by_index(client_index), "REMOTE_EXECUTION") == 0, "remote execution site mismatch");
    expect_true("controller remote host", host != NULL && strcmp(host, config.remote_host_label) == 0, "remote host mismatch");

    expect_true("controller destroy pair", edge_task_pair_destroy_by_task_index(client_index, EDGE_TASK_CLEANUP_PAIR) == 1, "controller cleanup failed");
    edge_offloader_shutdown();
    expect_true("controller shutdown clears config", edge_offloader_current_config() == NULL, "controller config should clear");
    expect_true("controller shutdown clears policy", edge_offloader_current_policy() == NULL, "controller policy should clear");

    pass("controller candidate collection and routing");
}

void test_offloader_policy_suite(void)
{
    printf("=== Offloader policy test harness ===\n");
    test_helpers_reset_counts();

    test_policy_null_inputs();
    test_policy_local_default();
    test_policy_remote_threshold();

    printf("=== Offloader policy tests done: passes=%" PRIu32 " fails=%" PRIu32 " ===\n",
           test_helpers_pass_count(),
           test_helpers_fail_count());
}

void test_offloader_controller_suite(void)
{
    printf("=== Offloader controller test harness ===\n");
    task_manager_init();

    test_controller_candidate_collection_and_routing();

    printf("=== Offloader controller tests done: passes=%" PRIu32 " fails=%" PRIu32 " ===\n",
           test_helpers_pass_count(),
           test_helpers_fail_count());
}
