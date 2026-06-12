/*
 * Offloader policy regression tests.
 *
 * Phase 3 currently covers the pure routing policy only. The controller
 * integration will arrive in later phases.
 */

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "core/offloader_policy.h"
#include "test_helpers.h"

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
