/**
 * @file ea_task_creation_example.c
 * @brief Example demonstrating dynamic edge task creation using the Task Manager.
 * 
 * This example creates a simple client-server pair of tasks that communicate via queues.
 * The client simulates reading sensor data and sending it to the server, which processes it.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "port/port_rtos.h"
#include "core/task_manager.h"

/* --- Task Definitions --- */

/* Client Task: Simulates reading a sensor and sending data */
void task_sensor_client(void *pvParameters)
{
    edge_task_params_t *params = (edge_task_params_t *)pvParameters;
    int sensor_data = 0;
    
    printf("[Client] Started. Params at %p\n", params);

    while (1) {
        sensor_data++;
        
        /* simulate work */
        eaPort_Delay_Milliseconds(500); 

        /* Send to Server */
        printf("[Client] Sending data: %d\n", sensor_data);
        if (eaPort_Queue_Send(params->queue_client_server, &sensor_data, 100) != eaPort_STATUS_OK) {
            printf("[Client] Queue full!\n");
        }


        int received_data;        /* Wait for processed data from Server */
        if (eaPort_Queue_Receive(params->queue_server_client, &received_data, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            printf("[Client] Processed data from server: %d\n", received_data);
            
            /* Simulate processing time */
            eaPort_Delay_Milliseconds(100);
            
        }
    }
}


/* Server Task: Receives data and processes it */
void task_processor_server(void *pvParameters)
{
    edge_task_params_t *params = (edge_task_params_t *)pvParameters;
    int received_data;
    int server_data = 1000; // Dummy data to send back

    printf("[Server] Started.\n");

    while (1) {
        /* Wait for data */
        if (eaPort_Queue_Receive(params->queue_client_server, &received_data, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK) {
            printf("[Server] Processed data: %d\n", received_data);
            
            /* Simulate processing time */
            eaPort_Delay_Milliseconds(100);

        }

        printf("[Server] Sending data: %d\n", server_data);
        if (eaPort_Queue_Send(params->queue_server_client, &server_data, 100) != eaPort_STATUS_OK) {
            printf("[Server] Queue full!\n");
        }
    }
}


/* --- Main Test Function --- */
void app_main(void)
{
    printf("=== Task Manager Test ===\n");

    /* 1. Initialize Manager */
    task_manager_init();

    /* 2. Create Dynamic Edge Task 
     * Create TWO tasks: "SensorTask-cl-0" and "SensorTask-sv-0"
     */
    printf("Creating Dynamic Edge Task...\n");
    
    int ret = CreateEATaskPinnedToCore(
        "SensorTask",           // Base Name
        2,                      // Priority
        task_sensor_client,     // Client Code
        task_processor_server,  // Server Code
        2048,                   // Stack Client
        2048,                   // Stack Server
        0,                      // Core ID
        ENRICHED,               // App Type (Must be != LOCAL to create pair)
        1000,                   // Deadline (ms)
        50,                     // Delay Sens
        50,                     // Energy Sens
        LOCAL_EXECUTION,        // Default Site (Forces server creation)
        "127.0.0.1",            // Host
        1000,                   // Period (ms)
        200,                    // WCET Client (ms)
        800                     // WCET Server (ms)
    );

    if (ret == 0) {
        printf("Dynamic Task Created Successfully.\n");
    } else {
        printf("Failed to create tasks.\n");
    }

    /* 3. Monitor Loop */
    while (1) {

        eaPort_Delay_Milliseconds(5000);
        printf("[HEARTBEAT] Ticks: %" PRIu32 "\n", (uint32_t)eaPort_Get_Tick_Time());
    }
}