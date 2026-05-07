#include "core/task_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h> 
#include "port/port_board.h"
#include "port/port_rtos.h"




edge_task_monitor_t monitoredTasks[CONFIG_EA_MAX_TASKS];
size_t numMonitoredTasks = 0;
static eaPort_mutex_t monitoredTasksMux = eaPort_MUTEX_INIT;
static const edge_task_pair_spec_t kDefaultPairSpec = {
    .queue_depth = 10U,
    .message_size = sizeof(void *),
};

void task_manager_init(void) {
    if (monitoredTasksMux == NULL) {
        monitoredTasksMux = eaPort_Mutex_Create();
        // Initialize monitored tasks array of size CONFIG_EA_MAX_TASKS
        // sets all entries to inactive
        memset(monitoredTasks, 0, sizeof(monitoredTasks));
        numMonitoredTasks = 0;
    }
}

static void ensure_initialized(void) {
    if (monitoredTasksMux == NULL) {
        task_manager_init();
    }
}


/**
 * @brief Helper to reconstruct the unique name used by the Task Manager.
 * Matches logic: "%s-%u" (Name-CoreID) or "%s-X" (No Affinity)
 */
static void format_unique_name(char* buffer, size_t size, const char* baseName, int coreID) {
    if (coreID == eaPort_NO_AFFINITY) {
        snprintf(buffer, size, "%s-X", baseName);
    } else {
        snprintf(buffer, size, "%s-%u", baseName, (unsigned)coreID);
    }
}


bool get_task_snapshot(const char* taskName, task_snapshot_t* out_snapshot)
{
    if (taskName == NULL || out_snapshot == NULL) return false;

    ensure_initialized();
    bool found = false;

    eaPort_Mutex_Enter(monitoredTasksMux);
    
    /* 1. Perform lookup INSIDE the lock */
    for (size_t i = 0; i < CONFIG_EA_MAX_TASKS; i++) {
        if (monitoredTasks[i].is_active && 
            strncmp(monitoredTasks[i].name, taskName, CONFIG_EA_MAX_TASK_NAME_LEN) == 0) {
            
            /* 2. Atomic Copy */
            out_snapshot->cpu_cycles = monitoredTasks[i].cpu_cycles;
            out_snapshot->period     = monitoredTasks[i].period;
            out_snapshot->WCET       = monitoredTasks[i].WCET;
            out_snapshot->OE2EL      = monitoredTasks[i].OE2EL;
            out_snapshot->host       = monitoredTasks[i].host;
            out_snapshot->handle     = monitoredTasks[i].task_handler;
            out_snapshot->valid      = true;
            
            found = true;
            break;
        }
    }
    
    eaPort_Mutex_Exit(monitoredTasksMux);
    
    return found;
}



int _CreateTaskPinnedToCore_(
    eaPort_task_function_t pxTaskCode, 
    const char *const pcName, 
    const uint32_t usStackDepth, 
    void *const pvParameters, 
    uint8_t uxPriority, 
    const uint8_t xCoreID, 
    edge_task_type_t app_type, 
    edge_task_segment_t app_segment, 
    unsigned MAE2EL, 
    uint8_t delay_weight, 
    uint8_t energy_weight, 
    const char *const pcHost, 
    const char *const pcRelation, 
    edge_task_execution_site_t pcExec,
    uint32_t xPeriod,
    unsigned WCET
)
{
    ensure_initialized();

    /* 2. Find a free slot (Critical Section - Short) */
    int slotIndex = -1;
    
    eaPort_Mutex_Enter(monitoredTasksMux);
    if (numMonitoredTasks < CONFIG_EA_MAX_TASKS) {
        for (int i = 0; i < CONFIG_EA_MAX_TASKS; i++) {
            // Find first empty slot (inactive)
            if (!monitoredTasks[i].is_active) {
                slotIndex = i;
                break;
            }
        }
    }
    eaPort_Mutex_Exit(monitoredTasksMux);

    if (slotIndex == -1) {
        printf("Error: Maximum number of tasks exceeded.\n");
        return -1;
    }

    /* 3. Create the OS Task (Expensive - Done OUTSIDE Mutex) */
    eaPort_task_t localTaskHandle = NULL;
    eaPort_status_t createStatus = eaPort_STATUS_ERROR;

    #if (configUSE_EDF_SCHEDULER == 1)

        // TODO: Create under EDF if needed (not implemented here)
    #else
        /* Standard creation using your Wrapper */
        createStatus = eaPort_Task_Create_Pinned_to_Core(
            pxTaskCode, 
            pcName, 
            usStackDepth, 
            pvParameters, 
            uxPriority, 
            &localTaskHandle, 
            xCoreID
        );
    #endif

    if (createStatus != eaPort_STATUS_OK) {
        printf("Error: Task creation failed.\n");
        return -1;
    }

    /* 4. Populate Array (Critical Section) */
    eaPort_Mutex_Enter(monitoredTasksMux);
    
    // Re-verify the slot is still ours (safeguard against weird race conditions)
    if (monitoredTasks[slotIndex].is_active) {
        // This should technically never happen if single-threaded creation, 
        // but good practice.
        eaPort_Mutex_Exit(monitoredTasksMux);
        eaPort_Task_Delete(localTaskHandle);
        return -1; 
    }

    edge_task_monitor_t *t = &monitoredTasks[slotIndex];

    /* -- Fill Strings -- */
    strncpy(t->name, pcName, CONFIG_EA_MAX_TASK_NAME_LEN - 1);
    t->name[CONFIG_EA_MAX_TASK_NAME_LEN - 1] = '\0';
    t->host = pcHost; 
    t->relation = pcRelation; // Pointing to base relation name is usually sufficient logic-wise

    /* -- Fill Metrics -- */
    t->task_handler = localTaskHandle;
    t->relation_handler = NULL; // To be set later if needed
    t->signal_request = SIGNAL_WAIT;
    t->type = app_type;
    t->segment = app_segment;
    t->exec_site = pcExec;
    t->core = xCoreID;
    
    t->MAE2EL = MAE2EL;
    t->WCET = WCET;
    t->period = xPeriod;
    t->delay_weight = delay_weight;
    t->energy_weight = energy_weight;

    t->cpu_cycles = 0;
    t->data_size = 0;
    t->OE2EL = 0;
    t->start_tick = 0;
    t->end_tick = 0;
    
    /* -- Mark Active -- */
    t->is_active = true;

    // Increment global counter if we appended
    if (slotIndex >= numMonitoredTasks) {
        numMonitoredTasks = slotIndex + 1;
    }

    eaPort_Mutex_Exit(monitoredTasksMux);

    return slotIndex;
}


int CreateEATaskPinnedToCore(
    const char *const TaskName, 
    uint8_t Priority, 
    eaPort_task_function_t TaskCodeClient, 
    eaPort_task_function_t TaskCodeServer, 
    const uint32_t MemStackDepthClient, 
    const uint32_t MemStackDepthServer, 
    const uint8_t CoreID, 
    edge_task_type_t AppType, 
    unsigned MAE2EL, 
    uint8_t DelaySensibility, 
    uint8_t EnergySensibility, 
    edge_task_execution_site_t DefaultExecutionSite, 
    const edge_task_pair_spec_t *pairSpec,
    const char *const HostName, 
    uint32_t xPeriod, 
    unsigned WCET_c, 
    unsigned WCET_s)
{
    int task_index = 0;
    const edge_task_pair_spec_t *resolvedSpec = pairSpec ? pairSpec : &kDefaultPairSpec;

  
    // Normalize Core ID for naming (handle -1 for no affinity)
    int actualCore = (CoreID == eaPort_NO_AFFINITY) ? 0 : CoreID; 


    if (resolvedSpec->queue_depth == 0U || resolvedSpec->message_size == 0U) {
        printf("Error: Invalid edge task pair specification.\n");
        return -1;
    }

    /* 1. Safe Allocation using Wrapper */
    edge_task_pair_runtime_t *edgeRuntime = (edge_task_pair_runtime_t *)eaPort_Malloc(sizeof(edge_task_pair_runtime_t));
    if (edgeRuntime == NULL) {
        printf("Error: Failed to allocate edge task runtime structure.\n");
        return -1;
    }
    memset(edgeRuntime, 0, sizeof(*edgeRuntime));

    /* 2. Queue Creation using Wrapper */
    edgeRuntime->queue_client_server = eaPort_Queue_Create(resolvedSpec->queue_depth, resolvedSpec->message_size);
    edgeRuntime->queue_server_client = eaPort_Queue_Create(resolvedSpec->queue_depth, resolvedSpec->message_size);

    if (edgeRuntime->queue_client_server == NULL || edgeRuntime->queue_server_client == NULL) {
        printf("Error: Queue creation failed.\n");
        goto error_cleanup;
    }

    /* 3. Handle enriched or remote Tasks */
    if (AppType != LOCAL) 
    {
        char clientBaseName[CONFIG_EA_MAX_TASK_NAME_LEN];
        char serverBaseName[CONFIG_EA_MAX_TASK_NAME_LEN];
        
        // Prepare base names: "Name-cl-CoreID", "Name-sv-CoreID"
        snprintf(clientBaseName, sizeof(clientBaseName), "%s-cl-%i", TaskName, actualCore);
        snprintf(serverBaseName, sizeof(serverBaseName), "%s-sv-%i", TaskName, actualCore);

        printf("Creation: Creating Edge Task: %s.\n", TaskName);

        /* --- A. Create Client Task --- */
        task_index = _CreateTaskPinnedToCore_(
            TaskCodeClient, 
            clientBaseName, 
            MemStackDepthClient, 
            (void *)edgeRuntime,
            Priority, 
            CoreID, 
            AppType, 
            CLIENT_SEGMENT, 
            MAE2EL, 
            DelaySensibility, 
            EnergySensibility, 
            HostName, 
            serverBaseName, // Relation
            DefaultExecutionSite,
            xPeriod,
            WCET_c
        );

        if (task_index < 0) {
            printf("Error: Client task creation failed.\n");
            goto error_cleanup;
        }

        //If no error in client task, store its handler
        edgeRuntime->HandlerClient = monitoredTasks[task_index].task_handler;
        /*If edge task is connected to Local as defalt*/
        //TODO: We should always create the task any how if it is enhanced, just suspend it 
        // after creatin if DefaultExecutionSite != LOCAL_EXECUTION

        /* --- B. Create Server Task (if Local Execution) --- */
        // TODO: We should always create the task, just suspend it if REMOTE
        if (DefaultExecutionSite == LOCAL_EXECUTION)
        {
            task_index = _CreateTaskPinnedToCore_(
                TaskCodeServer, 
                serverBaseName, 
                MemStackDepthServer, 
                (void *)edgeRuntime,
                Priority, 
                CoreID, 
                AppType, 
                SERVER_SEGMENT, 
                MAE2EL, 
                DelaySensibility, 
                EnergySensibility, 
                HostName, 
                clientBaseName, // Relation
                DefaultExecutionSite, 
                xPeriod,
                WCET_s
            );

            if (task_index < 0) {
                printf("Error: Server task creation failed.\n");
                // Need to clean up the client task created in step A
                eaPort_Task_Delete(edgeRuntime->HandlerClient);
                goto error_cleanup;
            }

            /* Retrieve Handle Safely */

            edgeRuntime->HandlerServer = monitoredTasks[task_index].task_handler;
        }
    }
    /* 4. Handle Local Tasks */
    else 
    {
        char localBaseName[CONFIG_EA_MAX_TASK_NAME_LEN];
        snprintf(localBaseName, sizeof(localBaseName), "%s-lc-%i", TaskName, actualCore);

        task_index = _CreateTaskPinnedToCore_(
            TaskCodeClient, 
            localBaseName, 
            MemStackDepthClient, 
            (void *)edgeRuntime,
            Priority, 
            CoreID, 
            AppType, 
            UNIQUE_SEGMENT, 
            MAE2EL, 
            DelaySensibility, 
            EnergySensibility, 
            "0.0.0.0", 
            "None", 
            LOCAL_EXECUTION, 
            xPeriod,
            WCET_c
        );

        if (task_index < 0) {
            printf("Error: Local task creation failed.\n");
            goto error_cleanup;
        }

        /* Retrieve Handle Safely */
        edgeRuntime->HandlerClient = monitoredTasks[task_index].task_handler;
    }

    return 0; // Success

/* 5. Centralized Error Handling */
error_cleanup:
    if (edgeRuntime) {
        if (edgeRuntime->queue_client_server) eaPort_Queue_Delete(edgeRuntime->queue_client_server);
        if (edgeRuntime->queue_server_client) eaPort_Queue_Delete(edgeRuntime->queue_server_client);
        eaPort_Free(edgeRuntime);
    }
    return -1;
}


// Helper function to compute the greatest common divisor
static uint32_t gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        uint32_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}


// Helper function to compute the least common multiple
static uint32_t lcm(uint32_t a, uint32_t b) {
    if (a == 0 || b == 0) return 0;
    return (a / gcd(a, b)) * b;
}

