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
    uint32_t       pair_id;
    int32_t        client_index;
    int32_t        server_index;
    uint8_t        lifecycle_state;
    char           client_name[CONFIG_EA_MAX_TASK_NAME_LEN];
    char           server_name[CONFIG_EA_MAX_TASK_NAME_LEN];
    char           host_name[CONFIG_EA_MAX_TASK_NAME_LEN];
    edge_task_pair_role_t role;
};

typedef enum {
    EDGE_RUNTIME_READY = 0,
    EDGE_RUNTIME_CLEANING = 1,
    EDGE_RUNTIME_RETIRED = 2,
} edge_runtime_lifecycle_t;

#define EDGE_TASK_RUNTIME_CHUNK_SIZE 4U

typedef struct edge_task_runtime_slot {
    edge_task_pair_runtime_t runtime;
    struct edge_task_runtime_slot *next_free;
    bool in_use;
} edge_task_runtime_slot_t;

typedef struct edge_task_runtime_chunk {
    struct edge_task_runtime_chunk *next;
    edge_task_runtime_slot_t slots[EDGE_TASK_RUNTIME_CHUNK_SIZE];
} edge_task_runtime_chunk_t;

static edge_task_runtime_chunk_t *runtimeChunks = NULL;
static edge_task_runtime_slot_t *runtimeFreeList = NULL;
static eaPort_mutex_t runtimeRegistryMux = eaPort_MUTEX_INIT;

static void runtime_registry_init(void);
static bool runtime_registry_expand(void);



static edge_task_monitor_hot_t monitoredTaskHot[CONFIG_EA_MAX_TASKS];
static edge_task_monitor_cold_t monitoredTaskCold[CONFIG_EA_MAX_TASKS];
size_t numMonitoredTasks = 0;
static eaPort_mutex_t monitoredTasksMux = eaPort_MUTEX_INIT;
static uint32_t nextPairId = 1U;

static edge_task_runtime_slot_t *runtime_slot_from_runtime(const edge_task_pair_runtime_t *runtime)
{
    if (runtime == NULL) {
        return NULL;
    }

    return (edge_task_runtime_slot_t *)((uint8_t *)runtime - offsetof(edge_task_runtime_slot_t, runtime));
}

static edge_task_pair_runtime_t *runtime_from_task_index(int taskIndex)
{
    if (taskIndex < 0) {
        return NULL;
    }

    runtime_registry_init();
    eaPort_Mutex_Enter(runtimeRegistryMux);
    for (edge_task_runtime_chunk_t *chunk = runtimeChunks; chunk != NULL; chunk = chunk->next) {
        for (size_t i = 0; i < EDGE_TASK_RUNTIME_CHUNK_SIZE; ++i) {
            edge_task_runtime_slot_t *slot = &chunk->slots[i];
            if (!slot->in_use) {
                continue;
            }

            if (slot->runtime.client_index == taskIndex || slot->runtime.server_index == taskIndex) {
                eaPort_Mutex_Exit(runtimeRegistryMux);
                return &slot->runtime;
            }
        }
    }
    eaPort_Mutex_Exit(runtimeRegistryMux);
    return NULL;
}

static void runtime_mark_retired(edge_task_pair_runtime_t *runtime)
{
    edge_task_runtime_slot_t *slot = runtime_slot_from_runtime(runtime);
    if (slot == NULL) {
        return;
    }

    runtime_registry_init();
    eaPort_Mutex_Enter(runtimeRegistryMux);
    if (slot->in_use) {
        slot->runtime.lifecycle_state = EDGE_RUNTIME_RETIRED;
    }
    eaPort_Mutex_Exit(runtimeRegistryMux);
}

static void cleanup_queue_handle(eaPort_queue_t *queue)
{
    if (queue != NULL && *queue != NULL) {
        eaPort_Queue_Delete(*queue);
        *queue = NULL;
    }
}

static void cleanup_task_handle(eaPort_task_t *handle)
{
    if (handle != NULL && *handle != NULL) {
        eaPort_Task_Delete(*handle);
        *handle = NULL;
    }
}

static void runtime_registry_init(void)
{
    if (runtimeRegistryMux == NULL) {
        runtimeRegistryMux = eaPort_Mutex_Create();
    }
}

static bool runtime_registry_expand(void)
{
    edge_task_runtime_chunk_t *chunk = (edge_task_runtime_chunk_t *)eaPort_Malloc(sizeof(*chunk));
    if (chunk == NULL) {
        return false;
    }

    memset(chunk, 0, sizeof(*chunk));
    chunk->next = runtimeChunks;
    runtimeChunks = chunk;

    for (size_t i = 0; i < EDGE_TASK_RUNTIME_CHUNK_SIZE; ++i) {
        chunk->slots[i].next_free = runtimeFreeList;
        runtimeFreeList = &chunk->slots[i];
    }

    return true;
}

static edge_task_pair_runtime_t *reserve_pair_runtime(void)
{
    runtime_registry_init();

    eaPort_Mutex_Enter(runtimeRegistryMux);
    if (runtimeFreeList == NULL && !runtime_registry_expand()) {
        eaPort_Mutex_Exit(runtimeRegistryMux);
        return NULL;
    }

    edge_task_runtime_slot_t *slot = runtimeFreeList;
    runtimeFreeList = slot->next_free;
    slot->next_free = NULL;
    slot->in_use = true;
    memset(&slot->runtime, 0, sizeof(slot->runtime));
    slot->runtime.lifecycle_state = EDGE_RUNTIME_READY;
    eaPort_Mutex_Exit(runtimeRegistryMux);

    return &slot->runtime;
}

static void release_pair_runtime_impl(edge_task_pair_runtime_t *runtime)
{
    if (runtime == NULL) {
        return;
    }

    edge_task_runtime_slot_t *slot = (edge_task_runtime_slot_t *)((uint8_t *)runtime - offsetof(edge_task_runtime_slot_t, runtime));

    runtime_registry_init();
    eaPort_Mutex_Enter(runtimeRegistryMux);
    if (slot->in_use) {
        slot->in_use = false;
        memset(&slot->runtime, 0, sizeof(slot->runtime));
        slot->next_free = runtimeFreeList;
        runtimeFreeList = slot;
    }
    eaPort_Mutex_Exit(runtimeRegistryMux);
}

static void record_monitor_from_task(
    edge_task_monitor_hot_t *hot,
    edge_task_monitor_cold_t *cold,
    const char *pcName,
    const char *pcHost,
    uint32_t pair_id,
    int32_t task_index,
    int32_t peer_index,
    edge_task_execution_site_t pcExec,
    uint8_t xCoreID,
    unsigned MAE2EL,
    uint8_t delay_weight,
    uint8_t energy_weight,
    uint32_t xPeriod,
    unsigned WCET)
{
    memset(hot, 0, sizeof(*hot));
    memset(cold, 0, sizeof(*cold));
    strncpy(cold->name, pcName, CONFIG_EA_MAX_TASK_NAME_LEN - 1);
    cold->name[CONFIG_EA_MAX_TASK_NAME_LEN - 1] = '\0';
    if (pcHost != NULL) {
        strncpy(cold->host, pcHost, CONFIG_EA_MAX_TASK_NAME_LEN - 1);
        cold->host[CONFIG_EA_MAX_TASK_NAME_LEN - 1] = '\0';
    }
    hot->pair_id = pair_id;
    hot->task_index = task_index;
    hot->peer_index = peer_index;
    cold->pair_id = pair_id;
    cold->task_index = task_index;
    cold->peer_index = peer_index;
    cold->exec_site = pcExec;
    cold->MAE2EL = MAE2EL;
    cold->WCET = WCET;
    cold->period = xPeriod;
    cold->delay_weight = delay_weight;
    cold->energy_weight = energy_weight;

    hot->signal_request = SIGNAL_WAIT;
    hot->core = xCoreID;
    hot->cpu_cycles = 0;
    hot->data_size = 0;
    hot->OE2EL = 0;
    hot->start_tick = 0;
    hot->end_tick = 0;
    hot->is_active = true;
}

static void update_monitor_linkage_by_index(size_t slot_index, uint32_t pair_id, int32_t task_index, int32_t peer_index)
{
    if (slot_index >= CONFIG_EA_MAX_TASKS) {
        return;
    }

    monitoredTaskHot[slot_index].pair_id = pair_id;
    monitoredTaskHot[slot_index].task_index = task_index;
    monitoredTaskHot[slot_index].peer_index = peer_index;
    monitoredTaskCold[slot_index].pair_id = pair_id;
    monitoredTaskCold[slot_index].task_index = task_index;
    monitoredTaskCold[slot_index].peer_index = peer_index;
}

static void clear_monitor_slot(size_t slot_index)
{
    if (slot_index >= CONFIG_EA_MAX_TASKS) {
        return;
    }

    bool was_active = monitoredTaskHot[slot_index].is_active;
    memset(&monitoredTaskHot[slot_index], 0, sizeof(monitoredTaskHot[slot_index]));
    memset(&monitoredTaskCold[slot_index], 0, sizeof(monitoredTaskCold[slot_index]));

    if (was_active && numMonitoredTasks > 0U) {
        --numMonitoredTasks;
    }
}

void edge_task_pair_runtime_release(edge_task_pair_runtime_t *runtime)
{
    release_pair_runtime_impl(runtime);
}

void task_manager_init(void) {
    if (monitoredTasksMux == NULL) {
        monitoredTasksMux = eaPort_Mutex_Create();
        runtime_registry_init();
        // Initialize monitored tasks array of size CONFIG_EA_MAX_TASKS
        // sets all entries to inactive
        memset(monitoredTaskHot, 0, sizeof(monitoredTaskHot));
        memset(monitoredTaskCold, 0, sizeof(monitoredTaskCold));
        numMonitoredTasks = 0;
        nextPairId = 1U;
    }
}

static void ensure_initialized(void) {
    if (monitoredTasksMux == NULL) {
        task_manager_init();
    }
}


static bool get_task_snapshot_locked(int taskIndex, task_snapshot_t* out_snapshot)
{
    if (out_snapshot == NULL || !monitor_index_valid(taskIndex)) {
        return false;
    }

    strncpy(out_snapshot->name, monitoredTaskCold[taskIndex].name, CONFIG_EA_MAX_TASK_NAME_LEN - 1);
    out_snapshot->name[CONFIG_EA_MAX_TASK_NAME_LEN - 1] = '\0';
    out_snapshot->cpu_cycles = monitoredTaskHot[taskIndex].cpu_cycles;
    out_snapshot->period     = monitoredTaskCold[taskIndex].period;
    out_snapshot->WCET       = monitoredTaskCold[taskIndex].WCET;
    out_snapshot->OE2EL      = monitoredTaskHot[taskIndex].OE2EL;
    out_snapshot->valid      = true;
    return true;
}

bool get_task_snapshot_by_index(int taskIndex, task_snapshot_t* out_snapshot)
{
    ensure_initialized();
    if (out_snapshot == NULL) {
        return false;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));

    eaPort_Mutex_Enter(monitoredTasksMux);
    bool found = get_task_snapshot_locked(taskIndex, out_snapshot);
    eaPort_Mutex_Exit(monitoredTasksMux);
    return found;
}

bool get_task_snapshot(const char* taskName, task_snapshot_t* out_snapshot)
{
    if (taskName == NULL || out_snapshot == NULL) return false;

    return get_task_snapshot_by_index(find_task_index(taskName), out_snapshot);
}

size_t get_num_monitored_tasks(void)
{
    ensure_initialized();

    eaPort_Mutex_Enter(monitoredTasksMux);
    size_t count = 0U;
    for (size_t i = 0; i < CONFIG_EA_MAX_TASKS; ++i) {
        if (monitoredTaskHot[i].is_active) {
            ++count;
        }
    }
    eaPort_Mutex_Exit(monitoredTasksMux);

    return count;
}

static bool monitor_index_valid(int taskIndex)
{
    return taskIndex >= 0 && taskIndex < (int)CONFIG_EA_MAX_TASKS && monitoredTaskHot[taskIndex].is_active;
}

static const char *execution_site_to_string(edge_task_execution_site_t site)
{
    switch (site) {
        case LOCAL_EXECUTION:
            return "LOCAL_EXECUTION";
        case REMOTE_EXECUTION:
            return "REMOTE_EXECUTION";
        default:
            return "UNKNOWN";
    }
}

int get_task_index(const char *taskName)
{
    return find_task_index(taskName);
}

int get_task_signal(int taskIndex)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return -1;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    int signal = (int)monitoredTaskHot[taskIndex].signal_request;
    eaPort_Mutex_Exit(monitoredTasksMux);
    return signal;
}

const char* get_task_ex_site_by_index(int taskIndex)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return "UNKNOWN";
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    const char *site = execution_site_to_string(monitoredTaskCold[taskIndex].exec_site);
    eaPort_Mutex_Exit(monitoredTasksMux);
    return site;
}

const char* get_task_ex_site(const char *taskName)
{
    ensure_initialized();
    if (taskName == NULL) {
        return "UNKNOWN";
    }

    return get_task_ex_site_by_index(find_task_index(taskName));
}

uint32_t get_task_cpu_cycles(int taskIndex)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return 0U;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    uint32_t value = monitoredTaskHot[taskIndex].cpu_cycles;
    eaPort_Mutex_Exit(monitoredTasksMux);
    return value;
}

uint32_t get_task_data_size(int taskIndex)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return 0U;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    uint32_t value = monitoredTaskHot[taskIndex].data_size;
    eaPort_Mutex_Exit(monitoredTasksMux);
    return value;
}

unsigned get_task_OE2EL(int taskIndex)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return 0U;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    unsigned value = monitoredTaskHot[taskIndex].OE2EL;
    eaPort_Mutex_Exit(monitoredTasksMux);
    return value;
}

unsigned get_task_WCET(int taskIndex)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return 0U;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    unsigned value = monitoredTaskCold[taskIndex].WCET;
    eaPort_Mutex_Exit(monitoredTasksMux);
    return value;
}

edge_task_monitor_sample_t start_task_monitoring(void)
{
    edge_task_monitor_sample_t sample = {0};
    sample.start_tick = (uint32_t)eaPort_Get_Tick_Time();
    sample.start_cycles = eaPort_Get_Cpu_Cycles();
    return sample;
}

int end_task_monitoring(edge_task_monitor_sample_t sample, uint32_t message_size)
{
    return end_task_monitoring_(sample, message_size, 0U, false);
}

int end_task_monitoring_(edge_task_monitor_sample_t sample, uint32_t message_size, uint32_t remote_time, bool remote_flag)
{
    (void)remote_time;
    (void)remote_flag;
    uint32_t end_tick = (uint32_t)eaPort_Get_Tick_Time();
    uint32_t end_cycles = eaPort_Get_Cpu_Cycles();
    uint32_t delta_cycles = (end_cycles >= sample.start_cycles) ? (end_cycles - sample.start_cycles) : 0U;
    return (int)(delta_cycles + message_size + (end_tick - sample.start_tick));
}

void check_self_suspend(int taskIndex)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    bool should_suspend = (monitoredTaskHot[taskIndex].signal_request == SIGNAL_SUSPEND);
    eaPort_Mutex_Exit(monitoredTasksMux);

    if (should_suspend) {
        eaPort_Task_Suspend(NULL);
    }
}

void update_task_metrics(const char *taskName, uint32_t newOE2EL, uint32_t newCycles, uint32_t newStartTick, uint32_t newEndTick, uint32_t newDataSize)
{
    int taskIndex = find_task_index(taskName);
    update_task_metrics_by_index(taskIndex, newOE2EL, newCycles, newStartTick, newEndTick, newDataSize);
}

void update_task_metrics_by_index(int taskIndex, uint32_t newOE2EL, uint32_t newCycles, uint32_t newStartTick, uint32_t newEndTick, uint32_t newDataSize)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    monitoredTaskHot[taskIndex].OE2EL = newOE2EL;
    monitoredTaskHot[taskIndex].cpu_cycles = newCycles;
    monitoredTaskHot[taskIndex].start_tick = newStartTick;
    monitoredTaskHot[taskIndex].end_tick = newEndTick;
    monitoredTaskHot[taskIndex].data_size = newDataSize;
    eaPort_Mutex_Exit(monitoredTasksMux);
}

void update_task_metrics_OE2EL(const char *taskName, uint32_t newOE2EL)
{
    update_task_metrics_OE2EL_by_index(find_task_index(taskName), newOE2EL);
}

void update_task_metrics_OE2EL_by_index(int taskIndex, uint32_t newOE2EL)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    monitoredTaskHot[taskIndex].OE2EL = newOE2EL;
    eaPort_Mutex_Exit(monitoredTasksMux);
}

int find_task_index(const char *taskName)
{
    ensure_initialized();
    if (taskName == NULL) {
        return -1;
    }

    /* Legacy compatibility lookup. New code should use task indices directly. */

    eaPort_Mutex_Enter(monitoredTasksMux);
    for (size_t i = 0; i < CONFIG_EA_MAX_TASKS; ++i) {
        if (monitoredTaskHot[i].is_active && strncmp(monitoredTaskCold[i].name, taskName, CONFIG_EA_MAX_TASK_NAME_LEN) == 0) {
            eaPort_Mutex_Exit(monitoredTasksMux);
            return (int)i;
        }
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
    return -1;
}

void remove_monitored_task(const char *taskName)
{
    remove_monitored_task_by_index(find_task_index(taskName));
}

void remove_monitored_task_by_index(int taskIndex)
{
    ensure_initialized();
    if (!monitor_index_valid(taskIndex)) {
        return;
    }

    clear_monitor_slot((size_t)taskIndex);
}

int is_client_task(const char* str)
{
    return (str != NULL && strstr(str, "-cl-") != NULL) ? 1 : 0;
}

uint32_t tasks_compute_hyperperiod(void)
{
    ensure_initialized();
    return 0U;
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

uint32_t edge_task_pair_id(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->pair_id : 0U;
}

int edge_task_pair_task_index(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->client_index : -1;
}

int edge_task_pair_peer_index(const edge_task_pair_runtime_t *runtime)
{
    return runtime ? runtime->server_index : -1;
}

static void clear_runtime_monitor_entries(int client_index, int server_index, bool include_server)
{
    eaPort_Mutex_Enter(monitoredTasksMux);
    if (client_index >= 0) {
        clear_monitor_slot((size_t)client_index);
    }
    if (include_server && server_index >= 0) {
        clear_monitor_slot((size_t)server_index);
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
}

int edge_task_pair_destroy(edge_task_pair_runtime_t *runtime, edge_task_cleanup_mode_t mode)
{
    if (runtime == NULL) {
        return 0;
    }

    runtime_registry_init();

    eaPort_task_t client_handle = NULL;
    eaPort_task_t server_handle = NULL;
    eaPort_task_t current_handle = eaPort_Get_Current_Task_Handle();
    eaPort_queue_t queue_client_server = NULL;
    eaPort_queue_t queue_server_client = NULL;
    int client_index = -1;
    int server_index = -1;
    bool full_teardown = false;

    eaPort_Mutex_Enter(runtimeRegistryMux);
    edge_task_runtime_slot_t *slot = runtime_slot_from_runtime(runtime);
    if (slot == NULL || !slot->in_use) {
        eaPort_Mutex_Exit(runtimeRegistryMux);
        return 1;
    }

    client_index = slot->runtime.client_index;
    server_index = slot->runtime.server_index;
    client_handle = slot->runtime.HandlerClient;
    server_handle = slot->runtime.HandlerServer;
    full_teardown = (mode == EDGE_TASK_CLEANUP_PAIR) || (server_handle == NULL && server_index < 0);

    if (slot->runtime.lifecycle_state == EDGE_RUNTIME_CLEANING) {
        eaPort_Mutex_Exit(runtimeRegistryMux);
        return 1;
    }

    if (full_teardown) {
        queue_client_server = slot->runtime.queue_client_server;
        queue_server_client = slot->runtime.queue_server_client;
        slot->runtime.queue_client_server = NULL;
        slot->runtime.queue_server_client = NULL;
        slot->runtime.HandlerClient = NULL;
        slot->runtime.HandlerServer = NULL;
        slot->runtime.client_index = -1;
        slot->runtime.server_index = -1;
        slot->runtime.lifecycle_state = EDGE_RUNTIME_CLEANING;
    } else {
        slot->runtime.HandlerClient = NULL;
        slot->runtime.client_index = -1;
    }

    eaPort_Mutex_Exit(runtimeRegistryMux);

    if (client_handle != NULL && client_handle != current_handle) {
        cleanup_task_handle(&client_handle);
    }
    if (full_teardown) {
        if (server_handle != NULL && server_handle != current_handle) {
            cleanup_task_handle(&server_handle);
        }
        cleanup_queue_handle(&queue_client_server);
        cleanup_queue_handle(&queue_server_client);
        clear_runtime_monitor_entries(client_index, server_index, true);
        runtime_mark_retired(runtime);
        release_pair_runtime_impl(runtime);
        if (client_handle != NULL && client_handle == current_handle) {
            eaPort_Task_Delete(client_handle);
        }
        if (server_handle != NULL && server_handle == current_handle) {
            eaPort_Task_Delete(server_handle);
        }
    } else {
        clear_runtime_monitor_entries(client_index, -1, false);
        if (client_handle != NULL && client_handle == current_handle) {
            eaPort_Task_Delete(client_handle);
        }
    }

    return 1;
}

int edge_task_pair_destroy_by_task_index(int taskIndex, edge_task_cleanup_mode_t mode)
{
    edge_task_pair_runtime_t *runtime = runtime_from_task_index(taskIndex);
    if (runtime == NULL) {
        return 1;
    }

    return edge_task_pair_destroy(runtime, mode);
}

int edge_task_pair_destroy_by_name(const char *taskName, edge_task_cleanup_mode_t mode)
{
    int taskIndex = find_task_index(taskName);
    if (taskIndex < 0) {
        return 1;
    }

    return edge_task_pair_destroy_by_task_index(taskIndex, mode);
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
    edge_task_pair_runtime_t *runtime,
    edge_task_execution_site_t pcExec,
    eaPort_task_t *outTaskHandle,
    uint32_t xPeriod,
    unsigned WCET
)
{
    ensure_initialized();
    (void)pcHost;
    (void)app_type;

    /* 2. Find a free slot (Critical Section - Short) */
    int slotIndex = -1;
    
    eaPort_Mutex_Enter(monitoredTasksMux);
    for (int i = 0; i < CONFIG_EA_MAX_TASKS; i++) {
        // Find first empty slot (inactive)
        if (!monitoredTaskHot[i].is_active) {
            slotIndex = i;
            break;
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
    if (monitoredTaskHot[slotIndex].is_active) {
        // This should technically never happen if single-threaded creation, 
        // but good practice.
        eaPort_Mutex_Exit(monitoredTasksMux);
        eaPort_Task_Delete(localTaskHandle);
        return -1; 
    }

    bool was_active = monitoredTaskHot[slotIndex].is_active;

    record_monitor_from_task(
        &monitoredTaskHot[slotIndex],
        &monitoredTaskCold[slotIndex],
        pcName,
        pcHost,
        runtime ? runtime->pair_id : 0U,
        slotIndex,
        (runtime != NULL)
            ? ((app_segment == CLIENT_SEGMENT) ? runtime->server_index : (app_segment == SERVER_SEGMENT) ? runtime->client_index : -1)
            : -1,
        pcExec,
        xCoreID,
        MAE2EL,
        delay_weight,
        energy_weight,
        xPeriod,
        WCET);

    // Keep the counter as an active-count, not a highest-index watermark.
    if (!was_active) {
        ++numMonitoredTasks;
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

    /* 1. Reserve a reusable runtime slot */
    edge_task_pair_runtime_t *edgeRuntime = reserve_pair_runtime();
    if (edgeRuntime == NULL) {
        printf("Error: Failed to reserve edge task runtime slot.\n");
        return -1;
    }
    edgeRuntime->pair_id = nextPairId++;
    edgeRuntime->client_index = -1;
    edgeRuntime->server_index = -1;
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
            edgeRuntime,
            DefaultExecutionSite,
            &edgeRuntime->HandlerClient,
            xPeriod,
            WCET_c
        );

        if (task_index < 0) {
            printf("Error: Client task creation failed.\n");
            goto error_cleanup;
        }
        edgeRuntime->client_index = task_index;
        update_monitor_linkage_by_index((size_t)task_index, edgeRuntime->pair_id, task_index, edgeRuntime->server_index);

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
                edgeRuntime,
                DefaultExecutionSite, 
                &edgeRuntime->HandlerServer,
                xPeriod,
                WCET_s
            );

            if (task_index < 0) {
                printf("Error: Server task creation failed.\n");
                // Need to clean up the client task created in step A
                cleanup_task_handle(&edgeRuntime->HandlerClient);
                clear_monitor_slot((size_t)edgeRuntime->client_index);
                goto error_cleanup;
            }
            edgeRuntime->server_index = task_index;
            update_monitor_linkage_by_index((size_t)edgeRuntime->client_index, edgeRuntime->pair_id, edgeRuntime->client_index, edgeRuntime->server_index);
            update_monitor_linkage_by_index((size_t)edgeRuntime->server_index, edgeRuntime->pair_id, edgeRuntime->server_index, edgeRuntime->client_index);

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
            edgeRuntime, 
            LOCAL_EXECUTION, 
            &edgeRuntime->HandlerClient,
            xPeriod,
            WCET_c
        );

        if (task_index < 0) {
            printf("Error: Local task creation failed.\n");
            goto error_cleanup;
        }
        edgeRuntime->client_index = task_index;
        update_monitor_linkage_by_index((size_t)task_index, edgeRuntime->pair_id, task_index, -1);

    }

    return 0; // Success

/* 5. Centralized Error Handling */
error_cleanup:
    if (edgeRuntime) {
        cleanup_queue_handle(&edgeRuntime->queue_client_server);
        cleanup_queue_handle(&edgeRuntime->queue_server_client);
        release_pair_runtime_impl(edgeRuntime);
    }
    return -1;
}
