#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "port/port_rtos.h"
#include "core/task_manager.h"

#define TASK_PAIR_QUEUE_DEPTH   1U
#define TASK_PAIR_MESSAGE_SIZE  sizeof(int)
#define TASK_PAIR_PERIOD_MS     1000U

static const edge_task_pair_spec_t kTaskPairSpec = {
    .queue_depth = TASK_PAIR_QUEUE_DEPTH,
    .message_size = TASK_PAIR_MESSAGE_SIZE,
};

static void task_sensor_client(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int sensor_data = 0;
    eaPort_queue_t out_queue = edge_task_pair_queue_client_to_server(runtime);
    eaPort_queue_t in_queue = edge_task_pair_queue_server_to_client(runtime);

    if (runtime == NULL || out_queue == NULL || in_queue == NULL) {
        printf("[Client] Missing runtime state.\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[Client] Started. Runtime at %p (pair=%" PRIu32 ", task=%d, peer=%d)\n",
           (void *)runtime,
           edge_task_pair_id(runtime),
           edge_task_pair_task_index(runtime),
           edge_task_pair_peer_index(runtime));

    while (1) {
        sensor_data++;
        eaPort_Delay_Milliseconds(500);

        printf("[Client] Sending data: %d\n", sensor_data);
        if (eaPort_Queue_Send(out_queue, &sensor_data, eaPort_NO_WAIT) != eaPort_STATUS_OK) {
            printf("[Client] Queue full!\n");
        }

        int received_data = 0;
        if (eaPort_Queue_Receive(in_queue, &received_data, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            printf("[Client] Processed data from server: %d\n", received_data);
            eaPort_Delay_Milliseconds(100);
        }
    }
}

static void task_processor_server(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int received_data = 0;
    const int server_data = 1000;
    eaPort_queue_t in_queue = edge_task_pair_queue_client_to_server(runtime);
    eaPort_queue_t out_queue = edge_task_pair_queue_server_to_client(runtime);

    if (runtime == NULL || in_queue == NULL || out_queue == NULL) {
        printf("[Server] Missing runtime state.\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[Server] Started. Runtime at %p (pair=%" PRIu32 ", task=%d, peer=%d)\n",
           (void *)runtime,
           edge_task_pair_id(runtime),
           edge_task_pair_task_index(runtime),
           edge_task_pair_peer_index(runtime));

    while (1) {
        if (eaPort_Queue_Receive(in_queue, &received_data, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            printf("[Server] Processed data: %d\n", received_data);
            eaPort_Delay_Milliseconds(100);
        }

        printf("[Server] Sending data: %d\n", server_data);
        if (eaPort_Queue_Send(out_queue, &server_data, eaPort_NO_WAIT) != eaPort_STATUS_OK) {
            printf("[Server] Queue full!\n");
        }
    }
}

void app_main(void)
{
    printf("=== Task Manager Test ===\n");
    task_manager_init();

    printf("Creating dynamic edge task pair...\n");
    int ret = CreateEATaskPinnedToCore(
        "SensorTask",
        2,
        task_sensor_client,
        task_processor_server,
        2048,
        2048,
        0,
        ENRICHED,
        1000,
        50,
        50,
        LOCAL_EXECUTION,
        &kTaskPairSpec,
        "127.0.0.1",
        TASK_PAIR_PERIOD_MS,
        200,
        800
    );

    if (ret == 0) {
        printf("Dynamic task pair created successfully.\n");
    } else {
        printf("Failed to create task pair.\n");
    }

    while (1) {
        eaPort_Delay_Milliseconds(5000);
        printf("[HEARTBEAT] Ticks: %" PRIu32 "\n", (uint32_t)eaPort_Get_Tick_Time());
    }
}
