/*
 * FreeRTOS-first onboarding regression tests.
 *
 * This harness keeps the hello-world example honest by checking that the
 * shortest public runtime path can start, create one enriched pair, expose a
 * status snapshot, and cleanly tear down again on real hardware.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_HELLO_PRIORITY        2U
#define TEST_HELLO_CORE_ID         0U
#define TEST_HELLO_STACK           2048U
#define TEST_HELLO_PERIOD_MS       1000U
#define TEST_HELLO_CLIENT_WCET     0U
#define TEST_HELLO_SERVER_WCET     0U

static const edge_task_pair_spec_t kHelloPairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static volatile bool g_hello_client_ready;
static volatile bool g_hello_server_ready;

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
    g_hello_client_ready = false;
    g_hello_server_ready = false;
}

static void hello_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;

    if (runtime != NULL) {
        g_hello_client_ready = true;
    }

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void hello_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;

    if (runtime != NULL) {
        g_hello_server_ready = true;
    }

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static edge_task_creation_result_t create_hello_pair(const char *task_name)
{
    edge_task_spec_t spec;

    edge_task_spec_init_enriched(
        &spec,
        task_name,
        hello_client_task,
        hello_server_task,
        TEST_HELLO_STACK,
        TEST_HELLO_STACK,
        TEST_HELLO_PERIOD_MS,
        &kHelloPairSpec);
    edge_task_spec_set_priority(&spec, TEST_HELLO_PRIORITY);
    edge_task_spec_set_core_id(&spec, TEST_HELLO_CORE_ID);
    edge_task_spec_set_local_host_label(&spec, "HELLO_LOCAL");
    edge_task_spec_set_wcet(&spec, TEST_HELLO_CLIENT_WCET, TEST_HELLO_SERVER_WCET);

    return CreateEATaskFromSpecEx(&spec);
}

static void test_hello_runtime_start_and_status(void)
{
    edge_runtime_status_t status = {0};

    edge_runtime_stop();
    expect_true("hello runtime null-safe", edge_runtime_status(NULL) == false, "status query should reject NULL");
    expect_true("hello runtime local-first start", edge_runtime_start_local_first("HELLO_LOCAL", "HELLO_REMOTE"),
                "local-first runtime start failed");
    expect_true("hello runtime state running", edge_runtime_state() == EDGE_RUNTIME_STATE_RUNNING,
                "runtime should be running");
    expect_true("hello runtime status available", edge_runtime_status(&status), "runtime status missing");
    expect_true("hello runtime configured", status.configured, "runtime should report configured");
    expect_true("hello runtime running", status.running, "runtime should report running");
    expect_true("hello runtime local label",
                status.local_host_label != NULL && strcmp(status.local_host_label, "HELLO_LOCAL") == 0,
                "local host label mismatch");
    expect_true("hello runtime remote label",
                status.remote_host_label != NULL && strcmp(status.remote_host_label, "HELLO_REMOTE") == 0,
                "remote host label mismatch");
    expect_true("hello runtime stop", edge_runtime_stop(), "runtime stop failed");

    pass("hello runtime startup");
}

static void test_hello_pair_creation_and_cleanup(void)
{
    const size_t baseline = get_num_monitored_tasks();
    edge_task_creation_result_t result = {0};
    edge_runtime_status_t status = {0};
    const edge_task_pair_runtime_t *runtime = NULL;

    reset_ready_flags();
    expect_true("hello runtime start", edge_runtime_start_local_first("HELLO_LOCAL", "HELLO_REMOTE"),
                "runtime start failed");
    result = create_hello_pair("HelloOnboarding");
    expect_true("hello pair create",
                result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("hello monitor count", get_num_monitored_tasks() == baseline + 2U,
                "hello onboarding should add two monitors");
    expect_true("hello client ready", wait_until(&g_hello_client_ready, 5000U), "client task did not start");
    expect_true("hello server ready", wait_until(&g_hello_server_ready, 5000U), "server task did not start");
    expect_true("hello runtime status", edge_runtime_status(&status), "runtime status unavailable after create");
    expect_true("hello runtime candidate count", status.client_candidates == 1U,
                "candidate count mismatch");

    runtime = edge_task_pair_runtime_by_task_index(result.task_index);
    expect_true("hello runtime lookup", runtime != NULL, "runtime lookup failed");
    expect_true("hello runtime label",
                edge_task_pair_local_host_label(runtime) != NULL &&
                    strcmp(edge_task_pair_local_host_label(runtime), "HELLO_LOCAL") == 0,
                "runtime host label mismatch");
    expect_true("hello cleanup", edge_task_pair_destroy_by_task_index(result.task_index, EDGE_TASK_CLEANUP_PAIR) == 1,
                "cleanup failed");
    expect_true("hello baseline restored", get_num_monitored_tasks() == baseline,
                "monitor baseline mismatch");
    expect_true("hello runtime stop", edge_runtime_stop(), "runtime stop failed");

    pass("hello pair creation and cleanup");
}

static void test_hello_repeated_start_stop(void)
{
    edge_runtime_status_t status = {0};

    expect_true("hello restart one", edge_runtime_start_local_first("HELLO_LOCAL", "HELLO_REMOTE"),
                "first restart failed");
    expect_true("hello restart one status", edge_runtime_status(&status) && status.running,
                "status should show running");
    expect_true("hello restart one stop", edge_runtime_stop(), "first restart stop failed");
    expect_true("hello restart two", edge_runtime_start_local_first("HELLO_LOCAL", "HELLO_REMOTE"),
                "second restart failed");
    expect_true("hello restart two status", edge_runtime_status(&status) && status.running,
                "second status should show running");
    expect_true("hello restart two stop", edge_runtime_stop(), "second restart stop failed");

    pass("hello repeated lifecycle");
}

void test_hello_world_onboarding_run(void)
{
    printf("=== Hello-world onboarding test harness ===\n");
    test_helpers_reset_counts();

    test_hello_runtime_start_and_status();
    test_hello_pair_creation_and_cleanup();
    test_hello_repeated_start_stop();

    printf("=== Hello-world onboarding tests done: passes=%u fails=%u ===\n",
           (unsigned)test_helpers_pass_count(),
           (unsigned)test_helpers_fail_count());
}
