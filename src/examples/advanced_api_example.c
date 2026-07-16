/*
 * Advanced task creation example.
 *
 * This sample intentionally uses the public task-spec helpers and accessors
 * so developers can see the full v1 contract with explicit overrides.
 */

#include <inttypes.h>
#include <stdio.h>

#include "examples/demo_examples.h"
#include "api/runtime.h"
#include "core/task_manager.h"
#include "port/port_rtos.h"

#define ADV_TASK_NAME           "AdvancedTask"
#define ADV_QUEUE_DEPTH         1U
#define ADV_MESSAGE_SIZE        sizeof(int)
#define ADV_PERIOD_MS           1000U
#define ADV_STACK_DEPTH         2048U
#define ADV_PRIORITY            2U
#define ADV_CORE_ID             0U
#define ADV_DEADLINE_MS         1200U
#define ADV_DELAY_WEIGHT        50U
#define ADV_ENERGY_WEIGHT       50U
#define ADV_CLIENT_WCET         200U
#define ADV_SERVER_WCET         800U

static const edge_task_pair_spec_t kAdvancedPairSpec = {
    .queue_depth = ADV_QUEUE_DEPTH,
    .message_size = ADV_MESSAGE_SIZE,
};

static void advanced_client_task(void *pvParameters)
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
        printf("[AdvancedClient] Missing runtime context\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[AdvancedClient] Ready (pair=%" PRIu32 ", task=%d, peer=%d, host=%s)\n",
           edge_task_pair_id(runtime),
           edge_task_pair_task_index(runtime),
           edge_task_pair_peer_index(runtime),
           edge_task_pair_local_host_label(runtime));

    while (1) {
        sensor_value++;
        printf("[AdvancedClient] Sending value: %d\n", sensor_value);

        if (eaPort_Queue_Send(out_queue, &sensor_value, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
            printf("[AdvancedClient] Failed to send sensor value\n");
        }

        int processed_value = 0;
        if (eaPort_Queue_Receive(in_queue, &processed_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            printf("[AdvancedClient] Received processed value: %d\n", processed_value);
        }

        eaPort_Delay_Milliseconds(500U);
    }
}

static void advanced_server_task(void *pvParameters)
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
        printf("[AdvancedServer] Missing runtime context\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[AdvancedServer] Ready (pair=%" PRIu32 ", task=%d, peer=%d, host=%s)\n",
           edge_task_pair_id(runtime),
           edge_task_pair_task_index(runtime),
           edge_task_pair_peer_index(runtime),
           edge_task_pair_local_host_label(runtime));

    while (1) {
        if (eaPort_Queue_Receive(in_queue, &received_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            int reply_value = received_value + 1000;
            printf("[AdvancedServer] Processing %d -> replying %d\n", received_value, reply_value);
            eaPort_Delay_Milliseconds(100U);

            if (eaPort_Queue_Send(out_queue, &reply_value, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
                printf("[AdvancedServer] Failed to send reply\n");
            }
        }
    }
}

static edge_task_spec_t make_advanced_task_spec(void)
{
    edge_task_spec_t spec;

    edge_task_spec_init_enriched(
        &spec,
        ADV_TASK_NAME,
        advanced_client_task,
        advanced_server_task,
        ADV_STACK_DEPTH,
        ADV_STACK_DEPTH,
        ADV_PERIOD_MS,
        &kAdvancedPairSpec);
    edge_task_spec_set_priority(&spec, ADV_PRIORITY);
    edge_task_spec_set_core_id(&spec, ADV_CORE_ID);
    edge_task_spec_set_local_host_label(&spec, "LOCAL_RUNTIME");
    edge_task_spec_set_deadline_ms(&spec, ADV_DEADLINE_MS);
    edge_task_spec_set_wcet(&spec, ADV_CLIENT_WCET, ADV_SERVER_WCET);
    edge_task_spec_set_execution_site_local(&spec);

    return spec;
}

static void log_advanced_snapshot(const edge_task_creation_result_t *creation)
{
    task_snapshot_t snapshot = {0};

    if (creation == NULL || creation->task_index < 0) {
        return;
    }

    if (get_task_snapshot_by_index(creation->task_index, &snapshot) && snapshot.valid) {
        printf("[Advanced] Snapshot: name=%s period=%" PRIu32 " wcet=%u oe2el=%u cycles=%" PRIu32 "\n",
               snapshot.name,
               snapshot.period,
               snapshot.WCET,
               snapshot.OE2EL,
               snapshot.cpu_cycles);
    }
}

void run_advanced_api_example(void)
{
    edge_runtime_config_t runtime_cfg;
    edge_runtime_status_t runtime_status = {0};
    edge_task_creation_result_t creation = {0};
    edge_task_spec_t spec = make_advanced_task_spec();

    printf("=== Advanced task API example ===\n");

    edge_runtime_config_init(&runtime_cfg);
    runtime_cfg.offloader.enabled = true;
    runtime_cfg.offloader.mode = EDGE_OFFLOADER_MODE_PER_TASK;
    runtime_cfg.offloader.control_period_ms = 100U;
    runtime_cfg.offloader.local_host_label = "LOCAL_RUNTIME";
    runtime_cfg.offloader.remote_host_label = "REMOTE_RUNTIME";

    if (!edge_runtime_start(&runtime_cfg)) {
        printf("[Advanced] Runtime start failed\n");
        return;
    }

    creation = CreateEATaskFromSpecEx(&spec);
    if (creation.failure_reason != EDGE_TASK_CREATION_FAILURE_NONE) {
        printf("[Advanced] Task creation failed: %s\n",
               edge_task_creation_failure_reason_to_string(creation.failure_reason));
        edge_runtime_stop();
        return;
    }

    if (edge_runtime_status(&runtime_status)) {
        printf("[Advanced] Runtime status: running=%d configured=%d monitored=%u candidates=%u policy=%s\n",
               runtime_status.running,
               runtime_status.configured,
               (unsigned)runtime_status.monitored_tasks,
               (unsigned)runtime_status.client_candidates,
               runtime_status.policy_name != NULL ? runtime_status.policy_name : "(null)");
        printf("[Advanced] Runtime labels: local=%s remote=%s\n",
               runtime_status.local_host_label != NULL ? runtime_status.local_host_label : "(null)",
               runtime_status.remote_host_label != NULL ? runtime_status.remote_host_label : "(null)");
    }

    log_advanced_snapshot(&creation);
    printf("[Advanced] Example is now running. Press reset to stop.\n");

    while (1) {
        eaPort_Delay_Milliseconds(5000U);

        if (edge_runtime_status(&runtime_status)) {
            printf("[Advanced] Heartbeat: running=%d monitored=%u candidates=%u policy=%s\n",
                   runtime_status.running,
                   (unsigned)runtime_status.monitored_tasks,
                   (unsigned)runtime_status.client_candidates,
                   runtime_status.policy_name != NULL ? runtime_status.policy_name : "(null)");
        }

        (void)edge_runtime_run_once();
    }
}
