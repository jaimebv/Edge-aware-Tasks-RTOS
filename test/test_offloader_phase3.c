/*
 * Phase 3 offloader policy contract tests.
 *
 * This harness focuses on the scheduler-aware policy selector, the built-in
 * fixed-priority and rate-monotonic policies, and the controller's failure
 * path when a policy rejects a plan.
 */

#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "core/offloader.h"
#include "core/offloader_policy.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_PHASE3_PRIORITY         2U
#define TEST_PHASE3_CORE_ID          0U
#define TEST_PHASE3_CLIENT_STACK     2048U
#define TEST_PHASE3_SERVER_STACK     2048U
#define TEST_PHASE3_PERIOD_MS        1000U
#define TEST_PHASE3_CLIENT_WCET      400U
#define TEST_PHASE3_SERVER_WCET      500U

static const edge_task_pair_spec_t kPhase3PairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static volatile bool g_phase3_client_ready;
static volatile bool g_phase3_server_ready;

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
    g_phase3_client_ready = false;
    g_phase3_server_ready = false;
}

static void phase3_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    if (runtime == NULL) {
        fail("phase3 client startup", "missing runtime");
        eaPort_Task_Delete(NULL);
        return;
    }

    g_phase3_client_ready = true;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void phase3_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    if (runtime == NULL) {
        fail("phase3 server startup", "missing runtime");
        eaPort_Task_Delete(NULL);
        return;
    }

    g_phase3_server_ready = true;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static edge_task_creation_result_t create_phase3_pair(const char *base_name)
{
    return CreateEATaskPinnedToCoreEx(
        base_name,
        TEST_PHASE3_PRIORITY,
        phase3_client_task,
        phase3_server_task,
        TEST_PHASE3_CLIENT_STACK,
        TEST_PHASE3_SERVER_STACK,
        TEST_PHASE3_CORE_ID,
        ENRICHED,
        TEST_PHASE3_PERIOD_MS,
        50U,
        50U,
        LOCAL_EXECUTION,
        &kPhase3PairSpec,
        "bootstrap-host",
        TEST_PHASE3_PERIOD_MS,
        TEST_PHASE3_CLIENT_WCET,
        TEST_PHASE3_SERVER_WCET);
}

static bool reject_eval(
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

static bool reject_plan(
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
    .name = "phase3-rejecting",
    .scheduler_policy = EDGE_OFFLOADER_SCHEDULER_CUSTOM,
    .evaluate = reject_eval,
    .plan = reject_plan,
};

static void test_policy_selection_defaults(void)
{
    const edge_offloader_policy_t *fp = edge_offloader_policy_default_for_scheduler(EDGE_OFFLOADER_SCHEDULER_FP);
    const edge_offloader_policy_t *rm = edge_offloader_policy_default_for_scheduler(EDGE_OFFLOADER_SCHEDULER_RM);
    const edge_offloader_policy_t *edf = edge_offloader_policy_default_for_scheduler(EDGE_OFFLOADER_SCHEDULER_EDF);
    const edge_offloader_policy_t *custom = edge_offloader_policy_default_for_scheduler(EDGE_OFFLOADER_SCHEDULER_CUSTOM);

    expect_true("fp policy present", fp != NULL, "default fp policy missing");
    expect_true("rm policy present", rm != NULL, "default rm policy missing");
    expect_true("edf fallback policy present", edf != NULL, "default edf policy missing");
    expect_true("custom fallback policy present", custom != NULL, "default custom policy missing");
    expect_true("fp policy name", strcmp(fp->name, "fixed-priority") == 0, "fp policy name mismatch");
    expect_true("rm policy name", strcmp(rm->name, "rate-monotonic") == 0, "rm policy name mismatch");
    expect_true("edf fallback policy name", strcmp(edf->name, "fixed-priority") == 0, "edf should fall back to fp");
    expect_true("custom fallback policy name", strcmp(custom->name, "fixed-priority") == 0, "custom should fall back to fp");
    expect_true("policy status string", strcmp(edge_offloader_policy_status_to_string(EDGE_OFFLOADER_POLICY_STATUS_UNSAFE_PLAN), "unsafe-plan") == 0,
                "policy status string mismatch");

    pass("policy selection defaults");
}

static void test_policy_route_split_by_scheduler(void)
{
    edge_offloader_candidate_t candidate = {0};
    edge_offloader_result_t result = {0};
    edge_offloader_policy_status_t status = EDGE_OFFLOADER_POLICY_STATUS_OK;
    const edge_offloader_policy_t *fp = edge_offloader_policy_fp();
    const edge_offloader_policy_t *rm = edge_offloader_policy_rm();

    candidate.task_index = 3;
    candidate.pair_id = 7U;
    candidate.snapshot.valid = true;
    candidate.snapshot.WCET = TEST_PHASE3_CLIENT_WCET;
    candidate.snapshot.period = TEST_PHASE3_PERIOD_MS;
    candidate.snapshot.OE2EL = 900U;
    candidate.runtime = NULL;

    expect_true("fp policy present", fp != NULL, "fp policy descriptor missing");
    expect_true("rm policy present", rm != NULL, "rm policy descriptor missing");

    expect_true("fp evaluate", fp->evaluate(NULL, &candidate, &result, &status), "fp evaluation failed");
    expect_true("fp status ok", status == EDGE_OFFLOADER_POLICY_STATUS_OK, "fp status mismatch");
    expect_true("fp route remote", result.route == EDGE_OFFLOADER_ROUTE_REMOTE, "fp should route remote");

    result = (edge_offloader_result_t){0};
    status = EDGE_OFFLOADER_POLICY_STATUS_OK;
    expect_true("rm evaluate", rm->evaluate(NULL, &candidate, &result, &status), "rm evaluation failed");
    expect_true("rm status ok", status == EDGE_OFFLOADER_POLICY_STATUS_OK, "rm status mismatch");
    expect_true("rm route local", result.route == EDGE_OFFLOADER_ROUTE_LOCAL, "rm should stay local");

    pass("policy route split by scheduler");
}

static void test_policy_rejects_invalid_snapshot(void)
{
    edge_offloader_candidate_t candidate = {0};
    edge_offloader_result_t result = {0};
    edge_offloader_policy_status_t status = EDGE_OFFLOADER_POLICY_STATUS_OK;
    const edge_offloader_policy_t *fp = edge_offloader_policy_fp();
    const edge_offloader_policy_t *rm = edge_offloader_policy_rm();

    candidate.task_index = 5;
    candidate.pair_id = 9U;
    candidate.snapshot.valid = false;
    candidate.snapshot.WCET = TEST_PHASE3_CLIENT_WCET;
    candidate.snapshot.period = TEST_PHASE3_PERIOD_MS;
    candidate.snapshot.OE2EL = 900U;
    candidate.runtime = NULL;

    expect_true("invalid snapshot fp reject", fp->evaluate(NULL, &candidate, &result, &status) == false,
                "fp should reject invalid snapshots");
    expect_true("invalid snapshot fp status", status == EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT,
                "fp should report invalid input");

    result = (edge_offloader_result_t){0};
    status = EDGE_OFFLOADER_POLICY_STATUS_OK;
    expect_true("invalid snapshot rm reject", rm->evaluate(NULL, &candidate, &result, &status) == false,
                "rm should reject invalid snapshots");
    expect_true("invalid snapshot rm status", status == EDGE_OFFLOADER_POLICY_STATUS_INVALID_INPUT,
                "rm should report invalid input");

    pass("policy rejects invalid snapshot");
}

static void test_controller_rejects_policy_failure_without_mutation(void)
{
    const edge_offloader_config_t config = {
        .enabled = true,
        .mode = EDGE_OFFLOADER_MODE_BATCH,
        .scheduler_policy = EDGE_OFFLOADER_SCHEDULER_CUSTOM,
        .control_period_ms = 100U,
        .local_host_label = "PHASE3_LOCAL",
        .remote_host_label = "PHASE3_REMOTE",
    };
    edge_task_creation_result_t creation = {0};
    const edge_task_pair_runtime_t *runtime = NULL;
    const char *host = NULL;

    reset_ready_flags();
    task_manager_init();
    edge_offloader_init(&config, &kRejectingPolicy);

    creation = create_phase3_pair("PHASE3");
    expect_true("phase3 pair create", creation.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && creation.task_index >= 0,
                edge_task_creation_failure_reason_to_string(creation.failure_reason));

    runtime = edge_task_pair_runtime_by_task_index(creation.task_index);
    expect_true("phase3 runtime present", runtime != NULL, "phase3 runtime missing");
    expect_true("phase3 client ready", wait_until(&g_phase3_client_ready, 5000U), "phase3 client task did not start");
    expect_true("phase3 server ready", wait_until(&g_phase3_server_ready, 5000U), "phase3 server task did not start");

    host = edge_task_pair_local_host_label(runtime);
    expect_true("phase3 bootstrap host", host != NULL && strcmp(host, "bootstrap-host") == 0, "phase3 host should start unchanged");
    expect_true("phase3 controller rejects plan", edge_offloader_run_once() == false, "rejecting policy should fail the controller");
    expect_true("phase3 host unchanged", strcmp(edge_task_pair_local_host_label(edge_task_pair_runtime_by_task_index(creation.task_index)), "bootstrap-host") == 0,
                "phase3 host should remain unchanged after policy rejection");
    expect_true("phase3 exec site unchanged", strcmp(get_task_ex_site_by_index(creation.task_index), "LOCAL_EXECUTION") == 0,
                "phase3 execution site should remain local after rejection");
    expect_true("phase3 cleanup", edge_task_pair_destroy_by_task_index(creation.task_index, EDGE_TASK_CLEANUP_PAIR) == 1,
                "phase3 cleanup failed");
    edge_offloader_shutdown();

    pass("controller rejects policy failure without mutation");
}

void test_offloader_phase3_suite(void)
{
    printf("=== Offloader phase 3 test harness ===\n");
    test_helpers_reset_counts();

    test_policy_selection_defaults();
    test_policy_route_split_by_scheduler();
    test_policy_rejects_invalid_snapshot();
    test_controller_rejects_policy_failure_without_mutation();

    printf("=== Offloader phase 3 tests done: passes=%" PRIu32 " fails=%" PRIu32 " ===\n",
           test_helpers_pass_count(),
           test_helpers_fail_count());
}
