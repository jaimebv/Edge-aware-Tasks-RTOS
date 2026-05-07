#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>

#include "core/task_manager.h"
#include "port/port_rtos.h"

#define DEMO_TASK_NAME          "SensorTask"
#define DEMO_QUEUE_DEPTH        1U
#define DEMO_MESSAGE_SIZE       sizeof(int)
#define DEMO_PERIOD_MS          1000U
#define DEMO_CLIENT_STACK       2048U
#define DEMO_SERVER_STACK       2048U
#define DEMO_PRIORITY           2U
#define DEMO_CORE_ID            0U
#define DEMO_MAE2EL_MS          1000U
#define DEMO_DELAY_SENSITIVITY  50U
#define DEMO_ENERGY_SENSITIVITY 50U
#define DEMO_CLIENT_WCET        200U
#define DEMO_SERVER_WCET        800U

static const edge_task_pair_spec_t kDemoPairSpec = {
    .queue_depth = DEMO_QUEUE_DEPTH,
    .message_size = DEMO_MESSAGE_SIZE,
};

static void demo_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int sensor_value = 0;
    eaPort_queue_t out_queue = edge_task_pair_queue_client_to_server(runtime);
    eaPort_queue_t in_queue = edge_task_pair_queue_server_to_client(runtime);

    if (runtime == NULL || out_queue == NULL || in_queue == NULL) {
        printf("[Client] Missing runtime context\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[Client] Ready\n");

    while (1) {
        sensor_value++;
        printf("[Client] Sending value: %d\n", sensor_value);

        if (eaPort_Queue_Send(out_queue, &sensor_value, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
            printf("[Client] Failed to send sensor value\n");
        }

        int processed_value = 0;
        if (eaPort_Queue_Receive(in_queue, &processed_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            printf("[Client] Received processed value: %d\n", processed_value);
        }

        eaPort_Delay_Milliseconds(500U);
    }
}

static void demo_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int received_value = 0;
    eaPort_queue_t in_queue = edge_task_pair_queue_client_to_server(runtime);
    eaPort_queue_t out_queue = edge_task_pair_queue_server_to_client(runtime);

    if (runtime == NULL || in_queue == NULL || out_queue == NULL) {
        printf("[Server] Missing runtime context\n");
        eaPort_Task_Delete(NULL);
        return;
    }

    printf("[Server] Ready\n");

    while (1) {
        if (eaPort_Queue_Receive(in_queue, &received_value, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            int reply_value = received_value + 1000;
            printf("[Server] Processing %d -> replying %d\n", received_value, reply_value);
            eaPort_Delay_Milliseconds(100U);

            if (eaPort_Queue_Send(out_queue, &reply_value, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
                printf("[Server] Failed to send reply\n");
            }
        }
    }
}

static void log_demo_status(void)
{
    printf("[Demo] Tick: %" PRIu32 "\n", (uint32_t)eaPort_Get_Tick_Time());
}

static void log_task_snapshot(const char *task_name)
{
    task_snapshot_t snapshot = {0};

    if (get_task_snapshot(task_name, &snapshot) && snapshot.valid) {
        printf("[Demo] %s => period=%" PRIu32 " WCET=%u OE2EL=%u cycles=%" PRIu32 "\n",
               snapshot.name[0] ? snapshot.name : task_name,
               snapshot.period,
               snapshot.WCET,
               snapshot.OE2EL,
               snapshot.cpu_cycles);
    } else {
        printf("[Demo] %s => snapshot unavailable\n", task_name);
    }
}

void app_main(void)
{
    printf("=== Edge-aware Tasks RTOS demo ===\n");
    task_manager_init();
    printf("[Demo] Task manager initialized\n");

    if (CreateEATaskPinnedToCore(
            DEMO_TASK_NAME,
            DEMO_PRIORITY,
            demo_client_task,
            demo_server_task,
            DEMO_CLIENT_STACK,
            DEMO_SERVER_STACK,
            DEMO_CORE_ID,
            ENRICHED,
            DEMO_MAE2EL_MS,
            DEMO_DELAY_SENSITIVITY,
            DEMO_ENERGY_SENSITIVITY,
            LOCAL_EXECUTION,
            &kDemoPairSpec,
            "127.0.0.1",
            DEMO_PERIOD_MS,
            DEMO_CLIENT_WCET,
            DEMO_SERVER_WCET) == 0) {
        printf("[Demo] Task pair created via framework API\n");
    } else {
        printf("[Demo] Task pair creation failed\n");
        while (1) {
            eaPort_Delay_Milliseconds(1000U);
            log_demo_status();
        }
    }

    printf("[Demo] Monitored tasks: %zu\n", get_num_monitored_tasks());
    log_task_snapshot("SensorTask-cl-0");
    log_task_snapshot("SensorTask-sv-0");
    log_demo_status();

    while (1) {
        eaPort_Delay_Milliseconds(5000U);
        log_demo_status();
    }
}
