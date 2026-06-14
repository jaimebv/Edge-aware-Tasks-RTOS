/*
 * Runtime facade example.
 *
 * This example shows the product-grade runtime API in the order a developer
 * would typically use it:
 *   1. configure the runtime
 *   2. define a public task spec
 *   3. start the runtime
 *   4. create the task pair through the public task model
 *   5. query runtime status
 *   6. run one controller cycle
 *   7. stop the runtime
 */

#include <stdio.h>

#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"

static const edge_task_pair_spec_t kExamplePairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static void example_client_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void example_server_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static edge_task_spec_t make_runtime_example_spec(void)
{
    edge_task_spec_t spec;

    edge_task_spec_init_enriched(
        &spec,
        "RuntimeApi",
        example_client_task,
        example_server_task,
        2048U,
        2048U,
        1000U,
        &kExamplePairSpec);
    edge_task_spec_set_priority(&spec, 2U);
    edge_task_spec_set_local_host_label(&spec, "LOCAL_RUNTIME");

    return spec;
}

void app_main(void)
{
    edge_runtime_status_t runtime_status = {0};
    edge_task_creation_result_t result = {0};
    edge_task_spec_t spec = make_runtime_example_spec();

    printf("=== Runtime facade example ===\n");

    if (!edge_runtime_start_local_first("LOCAL_RUNTIME", "REMOTE_RUNTIME")) {
        printf("Runtime start failed.\n");
        return;
    }

    result = CreateEATaskFromSpecEx(&spec);
    if (result.failure_reason != EDGE_TASK_CREATION_FAILURE_NONE) {
        printf("Task creation failed: %s\n",
               edge_task_creation_failure_reason_to_string(result.failure_reason));
        edge_runtime_stop();
        return;
    }

    if (edge_runtime_status(&runtime_status)) {
        printf("Runtime status: running=%d configured=%d monitored=%u candidates=%u policy=%s\n",
               runtime_status.running,
               runtime_status.configured,
               (unsigned)runtime_status.monitored_tasks,
               (unsigned)runtime_status.client_candidates,
               runtime_status.policy_name != NULL ? runtime_status.policy_name : "(null)");
    }

    (void)edge_runtime_run_once();
    (void)edge_task_pair_destroy_by_task_index(result.task_index, EDGE_TASK_CLEANUP_PAIR);
    (void)edge_runtime_stop();
}
