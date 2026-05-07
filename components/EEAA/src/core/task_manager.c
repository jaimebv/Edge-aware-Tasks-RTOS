#include "core/task_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h> 
#include "port/port_board.h"
#include "port/port_rtos.h"


struct edge_task_pair_runtime {
    eaPort_queue_t queue_client_server;
    eaPort_queue_t queue_server_client;
    eaPort_task_t  HandlerServer;
    eaPort_task_t  HandlerClient;
    char           client_name[CONFIG_EA_MAX_TASK_NAME_LEN];
    char           server_name[CONFIG_EA_MAX_TASK_NAME_LEN];
    char           host_name[CONFIG_EA_MAX_TASK_NAME_LEN];
    edge_task_pair_role_t role;
};



edge_task_monitor_t monitoredTasks[CONFIG_EA_MAX_TASKS];
size_t numMonitoredTasks = 0;
static eaPort_mutex_t monitoredTasksMux = eaPort_MUTEX_INIT;

static void record_monitor_from_task(
    edge_task_monitor_t *t,
    const char *pcName,
    edge_task_execution_site_t pcExec,
    uint8_t xCoreID,
    unsigned MAE2EL,
    uint8_t delay_weight,
    uint8_t energy_weight,
    uint32_t xPeriod,
    unsigned WCET)
{
    memset(t, 0, sizeof(*t));
    strncpy(t->name, pcName, CONFIG_EA_MAX_TASK_NAME_LEN - 1);
    t->name[CONFIG_EA_MAX_TASK_NAME_LEN - 1] = '\0';
    t->signal_request = SIGNAL_WAIT;
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
    t->is_active = true;
}

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


bool get_task_snapshot(const char* taskName, task_snapshot_t* out_snapshot)
{
    if (taskName == NULL || out_snapshot == NULL) return false;

    ensure_initialized();
    bool found = false;
    memset(out_snapshot, 0, sizeof(*out_snapshot));

    eaPort_Mutex_Enter(monitoredTasksMux);
    
    /* 1. Perform lookup INSIDE the lock */
    for (size_t i = 0; i < CONFIG_EA_MAX_TASKS; i++) {
        if (monitoredTasks[i].is_active && 
            strncmp(monitoredTasks[i].name, taskName, CONFIG_EA_MAX_TASK_NAME_LEN) == 0) {
            
            /* 2. Atomic Copy */
            strncpy(out_snapshot->name, monitoredTasks[i].name, CONFIG_EA_MAX_TASK_NAME_LEN - 1);
            out_snapshot->name[CONFIG_EA_MAX_TASK_NAME_LEN - 1] = '\0';
            out_snapshot->cpu_cycles = monitoredTasks[i].cpu_cycles;
            out_snapshot->period     = monitoredTasks[i].period;
            out_snapshot->WCET       = monitoredTasks[i].WCET;
            out_snapshot->OE2EL      = monitoredTasks[i].OE2EL;
            out_snapshot->valid      = true;
            
            found = true;
            break;
        }
    }
    
    eaPort_Mutex_Exit(monitoredTasksMux);
    
    return found;
}

size_t get_num_monitored_tasks(void)
{
    ensure_initialized();

    eaPort_Mutex_Enter(monitoredTasksMux);
    size_t count = numMonitoredTasks;
    eaPort_Mutex_Exit(monitoredTasksMux);

    return count;
}

eaPort_queue_t edge_task_pair_queue_client_to_server(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->queue_client_server : NULL;
}

eaPort_queue_t edge_task_pair_queue_server_to_client(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->queue_server_client : NULL;
}

eaPort_task_t edge_task_pair_client_handle(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->HandlerClient : NULL;
}

eaPort_task_t edge_task_pair_server_handle(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->HandlerServer : NULL;
}

const char *edge_task_pair_client_name(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->client_name : NULL;
}

const char *edge_task_pair_server_name(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->server_name : NULL;
}

const char *edge_task_pair_host_name(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->host_name : NULL;
}

edge_task_pair_role_t edge_task_pair_role(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->role : EDGE_TASK_PAIR_LOCAL;
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
    eaPort_task_t *outTaskHandle,
    uint32_t xPeriod,
    unsigned WCET
)
{
    ensure_initialized();
    (void)pcHost;
    (void)pcRelation;
    (void)app_type;
    (void)app_segment;

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
    record_monitor_from_task(
        t,
        pcName,
        pcExec,
        xCoreID,
        MAE2EL,
        delay_weight,
        energy_weight,
        xPeriod,
        WCET);

    // Increment global counter if we appended
    if (slotIndex >= numMonitoredTasks) {
        numMonitoredTasks = slotIndex + 1;
    }

    eaPort_Mutex_Exit(monitoredTasksMux);

    if (outTaskHandle != NULL) {
        *outTaskHandle = localTaskHandle;
    }

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
    const edge_task_pair_spec_t *resolvedSpec = pairSpec;

  
    // Normalize Core ID for naming (handle -1 for no affinity)
    int actualCore = (CoreID == eaPort_NO_AFFINITY) ? 0 : CoreID; 

    if (resolvedSpec == NULL) {
        printf("Error: edge task pair specification is required.\n");
        return -1;
    }

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
    if (HostName != NULL) {
        snprintf(edgeRuntime->host_name, sizeof(edgeRuntime->host_name), "%s", HostName);
    }
    edgeRuntime->role = EDGE_TASK_PAIR_LOCAL;

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

        snprintf(edgeRuntime->client_name, sizeof(edgeRuntime->client_name), "%s", clientBaseName);
        snprintf(edgeRuntime->server_name, sizeof(edgeRuntime->server_name), "%s", serverBaseName);
        edgeRuntime->role = EDGE_TASK_PAIR_CLIENT;

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
            &edgeRuntime->HandlerClient,
            xPeriod,
            WCET_c
        );

        if (task_index < 0) {
            printf("Error: Client task creation failed.\n");
            goto error_cleanup;
        }

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
                &edgeRuntime->HandlerServer,
                xPeriod,
                WCET_s
            );

            if (task_index < 0) {
                printf("Error: Server task creation failed.\n");
                // Need to clean up the client task created in step A
                eaPort_Task_Delete(edgeRuntime->HandlerClient);
                goto error_cleanup;
            }

        }
    }
    /* 4. Handle Local Tasks */
    else 
    {
        char localBaseName[CONFIG_EA_MAX_TASK_NAME_LEN];
        snprintf(localBaseName, sizeof(localBaseName), "%s-lc-%i", TaskName, actualCore);

        snprintf(edgeRuntime->client_name, sizeof(edgeRuntime->client_name), "%s", localBaseName);
        edgeRuntime->server_name[0] = '\0';
        edgeRuntime->role = EDGE_TASK_PAIR_LOCAL;

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
            &edgeRuntime->HandlerClient,
            xPeriod,
            WCET_c
        );

        if (task_index < 0) {
            printf("Error: Local task creation failed.\n");
            goto error_cleanup;
        }

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
