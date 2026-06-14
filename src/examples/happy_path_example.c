/*
 * Happy-path task creation example.
 *
 * This sample shows the short application flow for the common case:
 *   1. start the runtime with the local-first convenience helper
 *   2. declare a task pair with the happy-path constructor
 *   3. create the task through the public spec
 *   4. inspect runtime status
 *   5. clean up the task pair
 */

#include <inttypes.h>
#include <stdio.h>

#include "examples/demo_examples.h"
#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"

#define HAPPY_TASK_NAME         "HappyTask"
#define HAPPY_QUEUE_DEPTH       1U
#define HAPPY_MESSAGE_SIZE      sizeof(int)
#define HAPPY_PERIOD_MS         1000U
#define HAPPY_STACK_DEPTH       2048U
#define HAPPY_PRIORITY          2U
#define HAPPY_CLIENT_WCET       0U
#define HAPPY_SERVER_WCET       0U

static const edge_task_pair_spec_t kHappyPairSpec = {
    .queue_depth = HAPPY_QUEUE_DEPTH,
    .message_size = HAPPY_MESSAGE_SIZE,
};

static void happy_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int sensor_value = 0;
    eaPort_queue_t out_queue = NULL;
    eaPort_queue_t in_queue = NULL;

    if (runtime != NULL) {
        out_queue = edge_task_pair_queue_client_to_server(runtime);
        in_queue = edge_task_pair_queue_server_to_client(runtime);
    }

    if (runtime == NULL || out_queue == NULL || in_queue == NULL) {
        printf("[HappyClient] Missing runtime context\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[HappyClient] Ready (pair=%" PRIu32 ", task=%d, peer=%d, host=%s)\n",
           edge_task_pair_id(runtime),
           edge_task_pair_task_index(runtime),
           edge_task_pair_peer_index(runtime),
           edge_task_pair_local_host_label(runtime));

    while (1) {
        sensor_value++;
        printf("[HappyClient] Sending value: %d\n", sensor_value);

        if (eaPort_Queue_Send(out_queue, &sensor_value, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
            printf("[HappyClient] Failed to send sensor value\n");
        }

        int processed_value = 0;
        if (eaPort_Queue_Receive(in_queue, &processed_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            printf("[HappyClient] Received processed value: %d\n", processed_value);
        }

        eaPort_Delay_Milliseconds(500U);
    }
}

static void happy_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int received_value = 0;
    eaPort_queue_t in_queue = NULL;
    eaPort_queue_t out_queue = NULL;

    if (runtime != NULL) {
        in_queue = edge_task_pair_queue_client_to_server(runtime);
        out_queue = edge_task_pair_queue_server_to_client(runtime);
    }

    if (runtime == NULL || in_queue == NULL || out_queue == NULL) {
        printf("[HappyServer] Missing runtime context\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[HappyServer] Ready (pair=%" PRIu32 ", task=%d, peer=%d, host=%s)\n",
           edge_task_pair_id(runtime),
           edge_task_pair_task_index(runtime),
           edge_task_pair_peer_index(runtime),
           edge_task_pair_local_host_label(runtime));

    while (1) {
        if (eaPort_Queue_Receive(in_queue, &received_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            int reply_value = received_value + 1000;
            printf("[HappyServer] Processing %d -> replying %d\n", received_value, reply_value);
            eaPort_Delay_Milliseconds(100U);

            if (eaPort_Queue_Send(out_queue, &reply_value, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
                printf("[HappyServer] Failed to send reply\n");
            }
        }
    }
}

static edge_task_spec_t make_happy_task_spec(void)
{
    edge_task_spec_t spec;

    edge_task_spec_init_enriched(
        &spec,
        HAPPY_TASK_NAME,
        happy_client_task,
        happy_server_task,
        HAPPY_STACK_DEPTH,
        HAPPY_STACK_DEPTH,
        HAPPY_PERIOD_MS,
        &kHappyPairSpec);
    edge_task_spec_set_priority(&spec, HAPPY_PRIORITY);
    edge_task_spec_set_local_host_label(&spec, "LOCAL_RUNTIME");
    edge_task_spec_set_wcet(&spec, HAPPY_CLIENT_WCET, HAPPY_SERVER_WCET);

    return spec;
}

static void log_happy_snapshot(const edge_task_creation_result_t *creation)
{
    task_snapshot_t snapshot = {0};

    if (creation == NULL || creation->task_index < 0) {
        return;
    }

    if (get_task_snapshot_by_index(creation->task_index, &snapshot) && snapshot.valid) {
        printf("[Happy] Snapshot: name=%s period=%" PRIu32 " wcet=%u oe2el=%u cycles=%" PRIu32 "\n",
               snapshot.name,
               snapshot.period,
               snapshot.WCET,
               snapshot.OE2EL,
               snapshot.cpu_cycles);
    }
}

void run_happy_path_example(void)
{
    edge_runtime_status_t status = {0};
    edge_task_creation_result_t creation = {0};
    edge_task_spec_t spec = make_happy_task_spec();

    printf("=== Happy path task API example ===\n");

    if (!edge_runtime_start_local_first("LOCAL_RUNTIME", "REMOTE_RUNTIME")) {
        printf("[Happy] Runtime start failed\n");
        return;
    }

    creation = CreateEATaskFromSpecEx(&spec);
    if (creation.failure_reason != EDGE_TASK_CREATION_FAILURE_NONE) {
        printf("[Happy] Task creation failed: %s\n",
               edge_task_creation_failure_reason_to_string(creation.failure_reason));
        edge_runtime_stop();
        return;
    }

    if (edge_runtime_status(&status)) {
        printf("[Happy] Runtime status: running=%d configured=%d monitored=%u candidates=%u policy=%s\n",
               status.running,
               status.configured,
               (unsigned)status.monitored_tasks,
               (unsigned)status.client_candidates,
               status.policy_name != NULL ? status.policy_name : "(null)");
        printf("[Happy] Runtime labels: local=%s remote=%s\n",
               status.local_host_label != NULL ? status.local_host_label : "(null)",
               status.remote_host_label != NULL ? status.remote_host_label : "(null)");
    }

    log_happy_snapshot(&creation);
    printf("[Happy] Example is now running. Press reset to stop.\n");

    while (1) {
        eaPort_Delay_Milliseconds(5000U);

        if (edge_runtime_status(&status)) {
            printf("[Happy] Heartbeat: running=%d monitored=%u candidates=%u policy=%s\n",
                   status.running,
                   (unsigned)status.monitored_tasks,
                   (unsigned)status.client_candidates,
                   status.policy_name != NULL ? status.policy_name : "(null)");
        }

        (void)edge_runtime_run_once();
    }
}
