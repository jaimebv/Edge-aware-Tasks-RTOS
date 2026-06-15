#include <inttypes.h>
#include <stdio.h>

#include "examples/demo_examples.h"
#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"

#define HELLO_TASK_NAME    "HelloWorld"
#define HELLO_STACK_DEPTH  2048U
#define HELLO_PRIORITY     2U

static const edge_task_pair_spec_t kHelloPairSpec = {
    .queue_depth = 1U,
    .message_size = sizeof(int),
};

static void hello_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    eaPort_queue_t out_queue = NULL;
    eaPort_queue_t in_queue = NULL;

    if (runtime != NULL) {
        out_queue = edge_task_pair_queue_client_to_server(runtime);
        in_queue = edge_task_pair_queue_server_to_client(runtime);
    }

    if (runtime == NULL || out_queue == NULL || in_queue == NULL) {
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[HelloClient] Hello from the onboarding template (pair=%" PRIu32 ")\n",
           edge_task_pair_id(runtime));

    while (1) {
        int value = 1;
        int reply = 0;

        (void)eaPort_Queue_Send(out_queue, &value, eaPort_WAIT_FOREVER);
        (void)eaPort_Queue_Receive(in_queue, &reply, eaPort_WAIT_FOREVER);
        printf("[HelloClient] Heartbeat: reply=%d\n", reply);
        eaPort_Delay_Milliseconds(2000U);
    }
}

static void hello_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    eaPort_queue_t in_queue = NULL;
    eaPort_queue_t out_queue = NULL;

    if (runtime != NULL) {
        in_queue = edge_task_pair_queue_client_to_server(runtime);
        out_queue = edge_task_pair_queue_server_to_client(runtime);
    }

    if (runtime == NULL || in_queue == NULL || out_queue == NULL) {
        eaPort_Task_Delete(NULL);
        return;
    }

    while (1) {
        int received = 0;
        int reply = 0;

        if (eaPort_Queue_Receive(in_queue, &received, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            reply = received + 1;
            (void)eaPort_Queue_Send(out_queue, &reply, eaPort_WAIT_FOREVER);
        }
    }
}

static edge_task_spec_t make_hello_task_spec(void)
{
    edge_task_spec_t spec;

    edge_task_spec_init_enriched(
        &spec,
        HELLO_TASK_NAME,
        hello_client_task,
        hello_server_task,
        HELLO_STACK_DEPTH,
        HELLO_STACK_DEPTH,
        1000U,
        &kHelloPairSpec);
    edge_task_spec_set_priority(&spec, HELLO_PRIORITY);
    edge_task_spec_set_local_host_label(&spec, "LOCAL_RUNTIME");

    return spec;
}

void run_hello_world_example(void)
{
    edge_runtime_status_t status = {0};
    edge_task_creation_result_t creation = {0};
    edge_task_spec_t spec = make_hello_task_spec();

    if (!edge_runtime_start_local_first("LOCAL_RUNTIME", "REMOTE_RUNTIME")) {
        printf("[Hello] Runtime start failed\n");
        return;
    }

    creation = CreateEATaskFromSpecEx(&spec);
    if (creation.failure_reason != EDGE_TASK_CREATION_FAILURE_NONE) {
        printf("[Hello] Task creation failed\n");
        edge_runtime_stop();
        return;
    }

    if (edge_runtime_status(&status)) {
        printf("[Hello] running=%d monitored=%u candidates=%u policy=%s\n",
               status.running,
               (unsigned)status.monitored_tasks,
               (unsigned)status.client_candidates,
               status.policy_name != NULL ? status.policy_name : "(null)");
    }

    while (1) {
        eaPort_Delay_Milliseconds(5000U);
        printf("[Hello] Heartbeat: onboarding template is alive\n");
    }
}
