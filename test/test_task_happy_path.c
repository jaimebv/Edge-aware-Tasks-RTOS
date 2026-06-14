/*
 * Simplified happy-path API regression tests.
 *
 * These tests exercise the new convenience helpers that make the common
 * runtime/task creation flow shorter without removing the low-level API.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_HAPPY_PRIORITY        2U
#define TEST_HAPPY_CORE_ID         0U
#define TEST_HAPPY_STACK           2048U
#define TEST_HAPPY_PERIOD_MS       1000U
#define TEST_HAPPY_CLIENT_WCET     321U
#define TEST_HAPPY_SERVER_WCET     654U

static const edge_task_pair_spec_t kHappyPairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static void happy_enriched_client(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void happy_enriched_server(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void happy_local_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void test_runtime_happy_helpers(void)
{
    edge_runtime_status_t status = {0};

    edge_runtime_stop();
    expect_true("happy runtime default start", edge_runtime_start_default(), "default runtime start failed");
    expect_true("happy runtime state running", edge_runtime_state() == EDGE_RUNTIME_STATE_RUNNING, "runtime should be running");
    expect_true("happy runtime default stop", edge_runtime_stop(), "default runtime stop failed");

    expect_true("happy runtime local-first start",
                edge_runtime_start_local_first("LOCAL_RUNTIME", "REMOTE_RUNTIME"),
                "local-first runtime start failed");
    expect_true("happy runtime local-first status", edge_runtime_status(&status), "runtime status failed");
    expect_true("happy runtime local label",
                status.local_host_label != NULL && strcmp(status.local_host_label, "LOCAL_RUNTIME") == 0,
                "local host label mismatch");
    expect_true("happy runtime remote label",
                status.remote_host_label != NULL && strcmp(status.remote_host_label, "REMOTE_RUNTIME") == 0,
                "remote host label mismatch");
    expect_true("happy runtime stop", edge_runtime_stop(), "runtime stop failed");

    pass("happy runtime helpers");
}

static void test_task_enriched_helper(void)
{
    const size_t baseline = get_num_monitored_tasks();
    edge_task_spec_t spec;
    edge_task_creation_result_t result;
    const edge_task_pair_runtime_t *runtime = NULL;

    edge_task_spec_init_enriched(
        &spec,
        "HappyPair",
        happy_enriched_client,
        happy_enriched_server,
        TEST_HAPPY_STACK,
        TEST_HAPPY_STACK,
        TEST_HAPPY_PERIOD_MS,
        &kHappyPairSpec);
    edge_task_spec_set_priority(&spec, TEST_HAPPY_PRIORITY);
    edge_task_spec_set_core_id(&spec, TEST_HAPPY_CORE_ID);
    edge_task_spec_set_local_host_label(&spec, "LOCAL_RUNTIME");
    edge_task_spec_set_wcet(&spec, TEST_HAPPY_CLIENT_WCET, TEST_HAPPY_SERVER_WCET);
    edge_task_spec_set_execution_site_local(&spec);

    expect_true("happy enriched helper default execution site", spec.default_execution_site == LOCAL_EXECUTION,
                "enriched helper should default to local execution");
    expect_true("happy enriched helper deadline", spec.deadline_ms == TEST_HAPPY_PERIOD_MS,
                "enriched helper should default deadline to period");
    expect_true("happy enriched helper host label",
                spec.local_host_label != NULL && strcmp(spec.local_host_label, "LOCAL_RUNTIME") == 0,
                "enriched helper should default local host label");
    edge_task_spec_set_deadline_ms(&spec, 0U);
    expect_true("happy enriched helper deadline override", spec.deadline_ms == 0U,
                "deadline setter should allow zero for fallback behavior");
    expect_true("happy enriched helper valid", edge_task_spec_validate(&spec), "enriched helper spec should validate");

    result = CreateEATaskFromSpecEx(&spec);
    expect_true("happy enriched create",
                result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("happy enriched monitor count", get_num_monitored_tasks() == baseline + 2U,
                "enriched helper should add two monitors");
    eaPort_Delay_Milliseconds(20U);

    runtime = edge_task_pair_runtime_by_task_index(result.task_index);
    expect_true("happy enriched runtime", runtime != NULL, "enriched helper runtime missing");
    expect_true("happy enriched runtime label",
                edge_task_pair_local_host_label(runtime) != NULL &&
                    strcmp(edge_task_pair_local_host_label(runtime), "LOCAL_RUNTIME") == 0,
                "enriched helper runtime label mismatch");
    expect_true("happy enriched wcet", get_task_WCET(result.task_index) == TEST_HAPPY_CLIENT_WCET,
                "enriched helper WCET mismatch");
    expect_true("happy enriched cleanup", edge_task_pair_destroy_by_task_index(result.task_index, EDGE_TASK_CLEANUP_PAIR) == 1,
                "enriched helper cleanup failed");
    expect_true("happy enriched cleanup baseline", get_num_monitored_tasks() == baseline,
                "enriched helper cleanup should restore baseline");

    pass("happy enriched task helper");
}

static void test_task_local_helper(void)
{
    const size_t baseline = get_num_monitored_tasks();
    edge_task_spec_t spec;
    edge_task_creation_result_t result;
    const edge_task_pair_runtime_t *runtime = NULL;
    task_snapshot_t snapshot = {0};

    edge_task_spec_init_local(
        &spec,
        "HappyLocal",
        happy_local_task,
        TEST_HAPPY_STACK,
        TEST_HAPPY_PERIOD_MS,
        &kHappyPairSpec);
    edge_task_spec_set_priority(&spec, TEST_HAPPY_PRIORITY);
    edge_task_spec_set_core_id(&spec, TEST_HAPPY_CORE_ID);
    edge_task_spec_set_local_host_label(&spec, "LOCAL_RUNTIME");
    edge_task_spec_set_execution_site_local(&spec);

    expect_true("happy local helper app type", spec.app_type == LOCAL, "local helper should set LOCAL app type");
    expect_true("happy local helper deadline", spec.deadline_ms == TEST_HAPPY_PERIOD_MS,
                "local helper should default deadline to period");
    expect_true("happy local helper valid", edge_task_spec_validate(&spec), "local helper spec should validate");

    result = CreateEATaskFromSpecEx(&spec);
    expect_true("happy local create",
                result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("happy local monitor count", get_num_monitored_tasks() == baseline + 1U,
                "local helper should add one monitor");
    eaPort_Delay_Milliseconds(20U);

    runtime = edge_task_pair_runtime_by_task_index(result.task_index);
    expect_true("happy local runtime", runtime != NULL, "local helper runtime missing");
    expect_true("happy local runtime label",
                edge_task_pair_local_host_label(runtime) != NULL &&
                    strcmp(edge_task_pair_local_host_label(runtime), "LOCAL_RUNTIME") == 0,
                "local helper runtime label mismatch");
    expect_true("happy local snapshot", get_task_snapshot_by_index(result.task_index, &snapshot) && snapshot.valid,
                "local helper snapshot invalid");
    expect_true("happy local snapshot name", strstr(snapshot.name, "HappyLocal-lc-") != NULL,
                "local helper snapshot name mismatch");
    expect_true("happy local cleanup", edge_task_pair_destroy_by_task_index(result.task_index, EDGE_TASK_CLEANUP_CLIENT_ONLY) == 1,
                "local helper cleanup failed");
    expect_true("happy local cleanup baseline", get_num_monitored_tasks() == baseline,
                "local helper cleanup should restore baseline");

    pass("happy local task helper");
}

void test_task_happy_path_run(void)
{
    printf("=== Happy path task API test harness ===\n");
    test_helpers_reset_counts();
    edge_runtime_stop();

    test_runtime_happy_helpers();
    edge_runtime_stop();

    test_task_enriched_helper();
    edge_runtime_stop();

    test_task_local_helper();
    edge_runtime_stop();

    printf("=== Happy path task API tests done: passes=%u fails=%u ===\n",
           (unsigned)test_helpers_pass_count(),
           (unsigned)test_helpers_fail_count());
}
