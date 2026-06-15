/*
 * FreeRTOS-first hello-world onboarding example.
 *
 * This sample keeps the first board run intentionally small:
 *   1. start the runtime with the local-first convenience helper
 *   2. create one enriched task pair
 *   3. exchange a single hello payload
 *   4. print a compact status snapshot
 *   5. keep the board alive with a heartbeat
 */

#include <inttypes.h>
#include <stdio.h>

#include "examples/demo_examples.h"
#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"

#define HELLO_TASK_NAME         "HelloWorld"
#define HELLO_QUEUE_DEPTH       1U
#define HELLO_MESSAGE_SIZE      sizeof(int)
#define HELLO_PERIOD_MS         1000U
#define HELLO_STACK_DEPTH       2048U
#define HELLO_PRIORITY          2U

static const edge_task_pair_spec_t kHelloPairSpec = {
    .queue_depth = HELLO_QUEUE_DEPTH,
    .message_size = HELLO_MESSAGE_SIZE,
};

static void hello_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    eaPort_queue_t out_queue = NULL;
    eaPort_queue_t in_queue = NULL;
    int hello_value = 1;
    int reply_value = 0;

    if (runtime != NULL) {
        out_queue = edge_task_pair_queue_client_to_server(runtime);
        in_queue = edge_task_pair_queue_server_to_client(runtime);
    }

    if (runtime == NULL || out_queue == NULL || in_queue == NULL) {
        printf("[HelloClient] Missing runtime context\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[HelloClient] Hello from the FreeRTOS onboarding example (pair=%" PRIu32 ", task=%d, host=%s)\n",
           edge_task_pair_id(runtime),
           edge_task_pair_task_index(runtime),
           edge_task_pair_local_host_label(runtime));

    if (eaPort_Queue_Send(out_queue, &hello_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
        printf("[HelloClient] Sent hello payload: %d\n", hello_value);
    } else {
        printf("[HelloClient] Failed to send hello payload\n");
    }

    if (eaPort_Queue_Receive(in_queue, &reply_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
        printf("[HelloClient] Received reply payload: %d\n", reply_value);
    } else {
        printf("[HelloClient] Failed to receive reply payload\n");
    }

    while (1) {
        eaPort_Delay_Milliseconds(2000U);
        printf("[HelloClient] Heartbeat: onboarding demo is running\n");
    }
}

static void hello_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    eaPort_queue_t in_queue = NULL;
    eaPort_queue_t out_queue = NULL;
    int received_value = 0;

    if (runtime != NULL) {
        in_queue = edge_task_pair_queue_client_to_server(runtime);
        out_queue = edge_task_pair_queue_server_to_client(runtime);
    }

    if (runtime == NULL || in_queue == NULL || out_queue == NULL) {
        printf("[HelloServer] Missing runtime context\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[HelloServer] Ready for the onboarding exchange (pair=%" PRIu32 ", task=%d, host=%s)\n",
           edge_task_pair_id(runtime),
           edge_task_pair_task_index(runtime),
           edge_task_pair_local_host_label(runtime));

    while (1) {
        if (eaPort_Queue_Receive(in_queue, &received_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            int reply_value = received_value + 1;

            printf("[HelloServer] Processing %d -> replying %d\n", received_value, reply_value);

            if (eaPort_Queue_Send(out_queue, &reply_value, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
                printf("[HelloServer] Failed to send reply payload\n");
            }
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
        HELLO_PERIOD_MS,
        &kHelloPairSpec);
    edge_task_spec_set_priority(&spec, HELLO_PRIORITY);
    edge_task_spec_set_local_host_label(&spec, "LOCAL_RUNTIME");
    edge_task_spec_set_wcet(&spec, 0U, 0U);

    return spec;
}

static void log_hello_snapshot(const edge_task_creation_result_t *creation)
{
    task_snapshot_t snapshot = {0};

    if (creation == NULL || creation->task_index < 0) {
        return;
    }

    if (get_task_snapshot_by_index(creation->task_index, &snapshot) && snapshot.valid) {
        printf("[Hello] Snapshot: name=%s period=%" PRIu32 " wcet=%u oe2el=%u cycles=%" PRIu32 "\n",
               snapshot.name,
               snapshot.period,
               snapshot.WCET,
               snapshot.OE2EL,
               snapshot.cpu_cycles);
    }
}

void run_hello_world_example(void)
{
    edge_runtime_status_t status = {0};
    edge_task_creation_result_t creation = {0};
    edge_task_spec_t spec = make_hello_task_spec();

    printf("=== FreeRTOS-first hello-world onboarding example ===\n");

    if (!edge_runtime_start_local_first("LOCAL_RUNTIME", "REMOTE_RUNTIME")) {
        printf("[Hello] Runtime start failed\n");
        return;
    }

    creation = CreateEATaskFromSpecEx(&spec);
    if (creation.failure_reason != EDGE_TASK_CREATION_FAILURE_NONE) {
        printf("[Hello] Task creation failed: %s\n",
               edge_task_creation_failure_reason_to_string(creation.failure_reason));
        edge_runtime_stop();
        return;
    }

    if (edge_runtime_status(&status)) {
        printf("[Hello] Runtime status: running=%d configured=%d monitored=%u candidates=%u policy=%s\n",
               status.running,
               status.configured,
               (unsigned)status.monitored_tasks,
               (unsigned)status.client_candidates,
               status.policy_name != NULL ? status.policy_name : "(null)");
        printf("[Hello] Runtime labels: local=%s remote=%s\n",
               status.local_host_label != NULL ? status.local_host_label : "(null)",
               status.remote_host_label != NULL ? status.remote_host_label : "(null)");
    }

    log_hello_snapshot(&creation);
    printf("[Hello] Hello-world onboarding example is now running. Press reset to stop.\n");

    while (1) {
        eaPort_Delay_Milliseconds(5000U);

        if (edge_runtime_status(&status)) {
            printf("[Hello] Heartbeat: running=%d monitored=%u candidates=%u policy=%s\n",
                   status.running,
                   (unsigned)status.monitored_tasks,
                   (unsigned)status.client_candidates,
                   status.policy_name != NULL ? status.policy_name : "(null)");
        }
    }
}
