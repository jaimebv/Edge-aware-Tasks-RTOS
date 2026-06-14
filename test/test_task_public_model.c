/*
 * Public task model regression tests.
 *
 * Phase 1 introduces the developer-facing task spec and validates that the
 * public model maps into the existing task-manager creation path without
 * exposing internal runtime structures.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/task_manager.h"
#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_PUBLIC_MODEL_PRIORITY      2U
#define TEST_PUBLIC_MODEL_CORE_ID       0U
#define TEST_PUBLIC_MODEL_STACK         2048U
#define TEST_PUBLIC_MODEL_PERIOD_MS     1000U
#define TEST_PUBLIC_MODEL_DELAY_WEIGHT  50U
#define TEST_PUBLIC_MODEL_ENERGY_WEIGHT 50U

static const edge_task_pair_spec_t kPublicModelPairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static void public_model_local_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;

    if (runtime == NULL) {
        fail("public model local startup", "missing runtime");
        eaPort_Task_Delete(NULL);
        return;
    }

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void public_model_pair_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;

    if (runtime == NULL) {
        fail("public model client startup", "missing runtime");
        eaPort_Task_Delete(NULL);
        return;
    }

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void public_model_pair_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;

    if (runtime == NULL) {
        fail("public model server startup", "missing runtime");
        eaPort_Task_Delete(NULL);
        return;
    }

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void public_task_spec_init_and_validation(void)
{
    edge_task_spec_t spec;

    edge_task_spec_init(&spec);
    expect_true("public spec init default name", spec.task_name == NULL, "default task name should be NULL");
    expect_true("public spec init default local host label", spec.local_host_label != NULL && strcmp(spec.local_host_label, "LOCAL_RUNTIME") == 0,
                "default local host label should match runtime fallback");
    expect_true("public spec init default execution site", spec.default_execution_site == LOCAL_EXECUTION,
                "default execution site should be local");
    expect_true("public spec init default queue depth", spec.pair_spec.queue_depth == 1U, "default queue depth mismatch");
    expect_true("public spec init default queue size", spec.pair_spec.message_size == sizeof(int), "default queue size mismatch");
    expect_true("public spec init default deadline", spec.deadline_ms == 0U, "default deadline should be deferred to period");
    expect_true("public spec init default client wcet", spec.client_wcet == 0U, "default client WCET should be zero");
    expect_true("public spec init default server wcet", spec.server_wcet == 0U, "default server WCET should be zero");
    expect_true("public spec invalid when empty", edge_task_spec_validate(&spec) == false, "empty public spec should be invalid");

    spec.task_name = "PublicModel";
    spec.client_task_code = public_model_local_task;
    spec.server_task_code = public_model_local_task;
    spec.client_stack_depth = TEST_PUBLIC_MODEL_STACK;
    spec.server_stack_depth = TEST_PUBLIC_MODEL_STACK;
    spec.period_ms = TEST_PUBLIC_MODEL_PERIOD_MS;
    spec.pair_spec = kPublicModelPairSpec;

    expect_true("public spec valid after population", edge_task_spec_validate(&spec), "populated public spec should be valid");
    pass("public task spec validation");
}

static void public_task_model_local_creation(void)
{
    const size_t baseline = get_num_monitored_tasks();
    edge_task_spec_t spec;
    edge_task_creation_result_t result;
    task_snapshot_t snapshot = {0};

    edge_task_spec_init(&spec);
    spec.task_name = "PublicLocal";
    spec.priority = TEST_PUBLIC_MODEL_PRIORITY;
    spec.client_task_code = public_model_local_task;
    spec.server_task_code = public_model_local_task;
    spec.client_stack_depth = TEST_PUBLIC_MODEL_STACK;
    spec.server_stack_depth = TEST_PUBLIC_MODEL_STACK;
    spec.core_id = TEST_PUBLIC_MODEL_CORE_ID;
    spec.pair_spec = kPublicModelPairSpec;
    spec.period_ms = TEST_PUBLIC_MODEL_PERIOD_MS;
    spec.delay_weight = TEST_PUBLIC_MODEL_DELAY_WEIGHT;
    spec.energy_weight = TEST_PUBLIC_MODEL_ENERGY_WEIGHT;
    spec.local_host_label = "LOCAL_RUNTIME";

    expect_true("local public spec valid", edge_task_spec_validate(&spec), "local public spec should validate");

    result = CreateEATaskFromSpecEx(&spec);
    expect_true("local public create", result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("local public monitor count", get_num_monitored_tasks() == baseline + 1U, "local public task should add one monitor");
    eaPort_Delay_Milliseconds(20U);
    expect_true("local public snapshot valid", get_task_snapshot_by_index(result.task_index, &snapshot) && snapshot.valid, "local public snapshot invalid");
    expect_true("local public snapshot name", strncmp(snapshot.name, "PublicLocal-lc-", strlen("PublicLocal-lc-")) == 0,
                "local public snapshot name mismatch");
    expect_true("local public snapshot wcet default", snapshot.WCET == 0U, "local public snapshot WCET should default to zero");
    expect_true("local public cleanup", edge_task_pair_destroy_by_task_index(result.task_index, EDGE_TASK_CLEANUP_CLIENT_ONLY) == 1,
                "local public cleanup failed");
    expect_true("local public cleanup baseline", get_num_monitored_tasks() == baseline, "local public cleanup should restore baseline");
}

static void public_task_model_pair_creation(void)
{
    const size_t baseline = get_num_monitored_tasks();
    edge_task_spec_t spec;
    edge_task_creation_result_t result;
    const edge_task_pair_runtime_t *runtime = NULL;
    task_snapshot_t snapshot = {0};
    int client_index = -1;
    int server_index = -1;

    edge_task_spec_init(&spec);
    spec.task_name = "PublicPair";
    spec.priority = TEST_PUBLIC_MODEL_PRIORITY;
    spec.client_task_code = public_model_pair_client_task;
    spec.server_task_code = public_model_pair_server_task;
    spec.client_stack_depth = TEST_PUBLIC_MODEL_STACK;
    spec.server_stack_depth = TEST_PUBLIC_MODEL_STACK;
    spec.core_id = TEST_PUBLIC_MODEL_CORE_ID;
    spec.app_type = ENRICHED;
    spec.pair_spec = kPublicModelPairSpec;
    spec.period_ms = TEST_PUBLIC_MODEL_PERIOD_MS;
    spec.delay_weight = TEST_PUBLIC_MODEL_DELAY_WEIGHT;
    spec.energy_weight = TEST_PUBLIC_MODEL_ENERGY_WEIGHT;
    spec.local_host_label = "LOCAL_RUNTIME";

    expect_true("pair public spec valid", edge_task_spec_validate(&spec), "pair public spec should validate");

    result = CreateEATaskFromSpecEx(&spec);
    expect_true("pair public create", result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("pair public monitor count", get_num_monitored_tasks() == baseline + 2U, "pair public task should add two monitors");
    eaPort_Delay_Milliseconds(20U);

    client_index = result.task_index;
    runtime = edge_task_pair_runtime_by_task_index(client_index);
    expect_true("pair public runtime", runtime != NULL, "pair public runtime missing");
    server_index = edge_task_pair_peer_index(runtime);
    expect_true("pair public server index", server_index >= 0, "pair public server index missing");
    expect_true("pair public snapshot valid", get_task_snapshot_by_index(client_index, &snapshot) && snapshot.valid, "pair public snapshot invalid");
    expect_true("pair public snapshot name", strstr(snapshot.name, "PublicPair-cl-") != NULL, "pair public snapshot name mismatch");
    expect_true("pair public cleanup", edge_task_pair_destroy_by_task_index(client_index, EDGE_TASK_CLEANUP_PAIR) == 1,
                "pair public cleanup failed");
    expect_true("pair public cleanup baseline", get_num_monitored_tasks() == baseline, "pair public cleanup should restore baseline");
}

static void public_task_model_invalid_creation(void)
{
    edge_task_spec_t spec;
    edge_task_creation_result_t result;

    edge_task_spec_init(&spec);
    expect_true("null spec rejected", edge_task_spec_validate(NULL) == false, "NULL spec should be invalid");

    result = CreateEATaskFromSpecEx(&spec);
    expect_true("empty spec rejected", result.failure_reason == EDGE_TASK_CREATION_FAILURE_INVALID_SPEC,
                edge_task_creation_failure_reason_to_string(result.failure_reason));

    spec.task_name = "InvalidPublic";
    spec.client_task_code = public_model_local_task;
    spec.server_task_code = public_model_local_task;
    spec.client_stack_depth = 0U;
    spec.server_stack_depth = TEST_PUBLIC_MODEL_STACK;
    spec.period_ms = TEST_PUBLIC_MODEL_PERIOD_MS;
    spec.client_wcet = 10U;
    spec.server_wcet = 10U;

    expect_true("invalid stack rejected", edge_task_spec_validate(&spec) == false, "zero stack should be rejected");

    spec.client_stack_depth = TEST_PUBLIC_MODEL_STACK;
    spec.pair_spec.queue_depth = 0U;

    result = CreateEATaskFromSpecEx(&spec);
    expect_true("invalid queue rejected", result.failure_reason == EDGE_TASK_CREATION_FAILURE_INVALID_SPEC,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    pass("public task model invalid inputs");
}

void test_task_public_model_run(void)
{
    printf("=== Public task model test harness ===\n");
    test_helpers_reset_counts();
    task_manager_init();

    public_task_spec_init_and_validation();
    public_task_model_invalid_creation();
    public_task_model_local_creation();
    public_task_model_pair_creation();

    printf("=== Public task model tests done: passes=%u fails=%u ===\n",
           (unsigned)test_helpers_pass_count(),
           (unsigned)test_helpers_fail_count());
}
