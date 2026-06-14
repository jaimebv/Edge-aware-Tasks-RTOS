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
static bool runtime_registry_slot_owned(const edge_task_runtime_slot_t *slot);
static uint32_t next_pair_id_locked(void);
static void clear_runtime_monitor_entries(int client_index, int server_index, bool include_server);
static void cleanup_queue_handle(eaPort_queue_t *queue);
static void cleanup_task_handle(eaPort_task_t *handle);

__attribute__((weak)) bool edge_task_manager_test_hook_should_fail_creation(
    edge_task_creation_failure_reason_t reason,
    const char *task_name)
{
    (void)reason;
    (void)task_name;
    return false;
}

static void edge_task_spec_set_defaults(edge_task_spec_t *spec)
{
    if (spec == NULL) {
        return;
    }

    memset(spec, 0, sizeof(*spec));
    spec->priority = 1U;
    spec->core_id = eaPort_NO_AFFINITY;
    spec->app_type = LOCAL;
    spec->default_execution_site = LOCAL_EXECUTION;
    spec->host_name = "0.0.0.0";
    spec->pair_spec.queue_depth = 1U;
    spec->pair_spec.message_size = sizeof(int);
}

static bool edge_task_spec_is_valid(const edge_task_spec_t *spec)
{
    if (spec == NULL) {
        return false;
    }

    if (spec->task_name == NULL || spec->task_name[0] == '\0') {
        return false;
    }

    if (spec->client_task_code == NULL || spec->server_task_code == NULL) {
        return false;
    }

    if (spec->client_stack_depth == 0U || spec->server_stack_depth == 0U) {
        return false;
    }

    if (spec->pair_spec.queue_depth == 0U || spec->pair_spec.message_size == 0U) {
        return false;
    }

    if (spec->host_name == NULL || spec->host_name[0] == '\0') {
        return false;
    }

    if (spec->period_ms == 0U) {
        return false;
    }

    if (spec->client_wcet == 0U || spec->server_wcet == 0U) {
        return false;
    }

    return true;
}

static unsigned edge_task_spec_resolve_deadline_ms(const edge_task_spec_t *spec)
{
    if (spec == NULL) {
        return 0U;
    }

    if (spec->deadline_ms == 0U) {
        return spec->period_ms;
    }

    return spec->deadline_ms;
}

void edge_task_spec_init(edge_task_spec_t *spec)
{
    edge_task_spec_set_defaults(spec);
}

bool edge_task_spec_validate(const edge_task_spec_t *spec)
{
    return edge_task_spec_is_valid(spec);
}

static edge_task_creation_result_t edge_task_create_from_spec_impl(const edge_task_spec_t *spec)
{
    edge_task_creation_result_t result = {
        .task_index = -1,
        .failure_reason = EDGE_TASK_CREATION_FAILURE_NONE,
    };
    edge_task_spec_t resolved_spec;
    const char *host_name = NULL;

    if (!edge_task_spec_is_valid(spec)) {
        result.failure_reason = EDGE_TASK_CREATION_FAILURE_INVALID_SPEC;
        return result;
    }

    host_name = spec->host_name;

    resolved_spec = *spec;
    resolved_spec.deadline_ms = edge_task_spec_resolve_deadline_ms(spec);

    result = CreateEATaskPinnedToCoreEx(
        resolved_spec.task_name,
        resolved_spec.priority,
        resolved_spec.client_task_code,
        resolved_spec.server_task_code,
        resolved_spec.client_stack_depth,
        resolved_spec.server_stack_depth,
        resolved_spec.core_id,
        resolved_spec.app_type,
        resolved_spec.deadline_ms,
        resolved_spec.delay_weight,
        resolved_spec.energy_weight,
        resolved_spec.default_execution_site,
        &resolved_spec.pair_spec,
        host_name,
        resolved_spec.period_ms,
        resolved_spec.client_wcet,
        resolved_spec.server_wcet);

    return result;
}

edge_task_creation_result_t CreateEATaskFromSpecEx(const edge_task_spec_t *spec)
{
    return edge_task_create_from_spec_impl(spec);
}

int CreateEATaskFromSpec(const edge_task_spec_t *spec)
{
    edge_task_creation_result_t result = edge_task_create_from_spec_impl(spec);

    if (result.failure_reason != EDGE_TASK_CREATION_FAILURE_NONE) {
        printf("Error: Task creation failed (%s).\n", edge_task_creation_failure_reason_to_string(result.failure_reason));
        return -1;
    }

    return 0;
}



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

static edge_task_pair_runtime_t *runtime_from_task_index_locked(int taskIndex)
{
    if (taskIndex < 0) {
        return NULL;
    }

    for (edge_task_runtime_chunk_t *chunk = runtimeChunks; chunk != NULL; chunk = chunk->next) {
        for (size_t i = 0; i < EDGE_TASK_RUNTIME_CHUNK_SIZE; ++i) {
            edge_task_runtime_slot_t *slot = &chunk->slots[i];
            if (!slot->in_use || slot->runtime.lifecycle_state != EDGE_RUNTIME_READY) {
                continue;
            }

            if (slot->runtime.client_index == taskIndex || slot->runtime.server_index == taskIndex) {
                return &slot->runtime;
            }
        }
    }
    return NULL;
}

static edge_task_pair_runtime_t *runtime_from_task_index(int taskIndex)
{
    runtime_registry_init();
    eaPort_Mutex_Enter(runtimeRegistryMux);
    edge_task_pair_runtime_t *runtime = runtime_from_task_index_locked(taskIndex);
    eaPort_Mutex_Exit(runtimeRegistryMux);
    return runtime;
}

static void runtime_mark_retired(edge_task_pair_runtime_t *runtime)
{
    edge_task_runtime_slot_t *slot = runtime_slot_from_runtime(runtime);
    if (slot == NULL) {
        return;
    }

    runtime_registry_init();
    eaPort_Mutex_Enter(runtimeRegistryMux);
    if (runtime_registry_slot_owned(slot) && slot->in_use) {
        slot->runtime.lifecycle_state = EDGE_RUNTIME_RETIRED;
    }
    eaPort_Mutex_Exit(runtimeRegistryMux);
}

static void runtime_mark_cleaning(edge_task_pair_runtime_t *runtime)
{
    edge_task_runtime_slot_t *slot = runtime_slot_from_runtime(runtime);
    if (slot == NULL) {
        return;
    }

    runtime_registry_init();
    eaPort_Mutex_Enter(runtimeRegistryMux);
    if (runtime_registry_slot_owned(slot) && slot->in_use) {
        slot->runtime.lifecycle_state = EDGE_RUNTIME_CLEANING;
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

const char *edge_task_creation_failure_reason_to_string(edge_task_creation_failure_reason_t reason)
{
    switch (reason) {
        case EDGE_TASK_CREATION_FAILURE_NONE:
            return "none";
        case EDGE_TASK_CREATION_FAILURE_INVALID_SPEC:
            return "invalid-spec";
        case EDGE_TASK_CREATION_FAILURE_RUNTIME_SLOT:
            return "runtime-slot";
        case EDGE_TASK_CREATION_FAILURE_QUEUE_CLIENT:
            return "queue-client";
        case EDGE_TASK_CREATION_FAILURE_QUEUE_SERVER:
            return "queue-server";
        case EDGE_TASK_CREATION_FAILURE_CLIENT_TASK:
            return "client-task";
        case EDGE_TASK_CREATION_FAILURE_SERVER_TASK:
            return "server-task";
        case EDGE_TASK_CREATION_FAILURE_LOCAL_TASK:
            return "local-task";
        default:
            return "unknown";
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

static bool runtime_registry_slot_owned(const edge_task_runtime_slot_t *slot)
{
    if (slot == NULL) {
        return false;
    }

    for (edge_task_runtime_chunk_t *chunk = runtimeChunks; chunk != NULL; chunk = chunk->next) {
        for (size_t i = 0; i < EDGE_TASK_RUNTIME_CHUNK_SIZE; ++i) {
            if (&chunk->slots[i] == slot) {
                return true;
            }
        }
    }

    return false;
}

static uint32_t next_pair_id_locked(void)
{
    uint32_t pair_id = nextPairId++;
    if (nextPairId == 0U) {
        nextPairId = 1U;
    }
    if (pair_id == 0U) {
        pair_id = nextPairId++;
    }
    return pair_id;
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
    if (runtime_registry_slot_owned(slot) && slot->in_use) {
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
    unsigned deadline_ms,
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
    cold->deadline_ms = deadline_ms;
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

    eaPort_Mutex_Enter(monitoredTasksMux);
    monitoredTaskHot[slot_index].pair_id = pair_id;
    monitoredTaskHot[slot_index].task_index = task_index;
    monitoredTaskHot[slot_index].peer_index = peer_index;
    monitoredTaskCold[slot_index].pair_id = pair_id;
    monitoredTaskCold[slot_index].task_index = task_index;
    monitoredTaskCold[slot_index].peer_index = peer_index;
    eaPort_Mutex_Exit(monitoredTasksMux);
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

static bool monitor_index_valid_locked(int taskIndex);

static bool get_task_snapshot_locked(int taskIndex, task_snapshot_t* out_snapshot)
{
    if (out_snapshot == NULL || !monitor_index_valid_locked(taskIndex)) {
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

static bool monitor_index_valid_locked(int taskIndex)
{
    return taskIndex >= 0 && taskIndex < (int)CONFIG_EA_MAX_TASKS && monitoredTaskHot[taskIndex].is_active;
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
    eaPort_Mutex_Enter(monitoredTasksMux);
    int signal = -1;
    if (monitor_index_valid_locked(taskIndex)) {
        signal = (int)monitoredTaskHot[taskIndex].signal_request;
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
    return signal;
}

const char* get_task_ex_site_by_index(int taskIndex)
{
    ensure_initialized();
    eaPort_Mutex_Enter(monitoredTasksMux);
    const char *site = "UNKNOWN";
    if (monitor_index_valid_locked(taskIndex)) {
        site = execution_site_to_string(monitoredTaskCold[taskIndex].exec_site);
    }
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
    eaPort_Mutex_Enter(monitoredTasksMux);
    uint32_t value = 0U;
    if (monitor_index_valid_locked(taskIndex)) {
        value = monitoredTaskHot[taskIndex].cpu_cycles;
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
    return value;
}

uint32_t get_task_data_size(int taskIndex)
{
    ensure_initialized();
    eaPort_Mutex_Enter(monitoredTasksMux);
    uint32_t value = 0U;
    if (monitor_index_valid_locked(taskIndex)) {
        value = monitoredTaskHot[taskIndex].data_size;
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
    return value;
}

unsigned get_task_OE2EL(int taskIndex)
{
    ensure_initialized();
    eaPort_Mutex_Enter(monitoredTasksMux);
    unsigned value = 0U;
    if (monitor_index_valid_locked(taskIndex)) {
        value = monitoredTaskHot[taskIndex].OE2EL;
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
    return value;
}

unsigned get_task_WCET(int taskIndex)
{
    ensure_initialized();
    eaPort_Mutex_Enter(monitoredTasksMux);
    unsigned value = 0U;
    if (monitor_index_valid_locked(taskIndex)) {
        value = monitoredTaskCold[taskIndex].WCET;
    }
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
    eaPort_Mutex_Enter(monitoredTasksMux);
    bool should_suspend = monitor_index_valid_locked(taskIndex) && (monitoredTaskHot[taskIndex].signal_request == SIGNAL_SUSPEND);
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
    eaPort_Mutex_Enter(monitoredTasksMux);
    if (monitor_index_valid_locked(taskIndex)) {
        monitoredTaskHot[taskIndex].OE2EL = newOE2EL;
        monitoredTaskHot[taskIndex].cpu_cycles = newCycles;
        monitoredTaskHot[taskIndex].start_tick = newStartTick;
        monitoredTaskHot[taskIndex].end_tick = newEndTick;
        monitoredTaskHot[taskIndex].data_size = newDataSize;
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
}

void update_task_metrics_OE2EL(const char *taskName, uint32_t newOE2EL)
{
    update_task_metrics_OE2EL_by_index(find_task_index(taskName), newOE2EL);
}

void update_task_metrics_OE2EL_by_index(int taskIndex, uint32_t newOE2EL)
{
    ensure_initialized();
    eaPort_Mutex_Enter(monitoredTasksMux);
    if (monitor_index_valid_locked(taskIndex)) {
        monitoredTaskHot[taskIndex].OE2EL = newOE2EL;
    }
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
    eaPort_Mutex_Enter(monitoredTasksMux);
    if (monitor_index_valid_locked(taskIndex)) {
        clear_monitor_slot((size_t)taskIndex);
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
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

const edge_task_pair_runtime_t *edge_task_pair_runtime_by_task_index(int taskIndex)
{
    return runtime_from_task_index(taskIndex);
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

static bool update_monitor_host_locked(int taskIndex, const char *host)
{
    if (host == NULL || host[0] == '\0') {
        return false;
    }

    eaPort_Mutex_Enter(monitoredTasksMux);
    if (monitor_index_valid_locked(taskIndex)) {
        strncpy(monitoredTaskCold[taskIndex].host, host, CONFIG_EA_MAX_TASK_NAME_LEN - 1U);
        monitoredTaskCold[taskIndex].host[CONFIG_EA_MAX_TASK_NAME_LEN - 1U] = '\0';
        eaPort_Mutex_Exit(monitoredTasksMux);
        return true;
    }
    eaPort_Mutex_Exit(monitoredTasksMux);
    return false;
}

static bool update_monitor_exec_site_locked(int taskIndex, edge_task_execution_site_t exec_site)
{
    bool updated = false;

    eaPort_Mutex_Enter(monitoredTasksMux);
    if (monitor_index_valid_locked(taskIndex)) {
        monitoredTaskCold[taskIndex].exec_site = exec_site;
        updated = true;
    }
    eaPort_Mutex_Exit(monitoredTasksMux);

    return updated;
}

bool edge_task_pair_set_host_by_index(int taskIndex, const char *host)
{
    bool updated = update_monitor_host_locked(taskIndex, host);

    if (host == NULL || host[0] == '\0') {
        return false;
    }

    runtime_registry_init();
    eaPort_Mutex_Enter(runtimeRegistryMux);
    edge_task_pair_runtime_t *runtime = runtime_from_task_index_locked(taskIndex);
    if (runtime != NULL) {
        snprintf(runtime->host_name, sizeof(runtime->host_name), "%s", host);
        updated = true;
    }
    eaPort_Mutex_Exit(runtimeRegistryMux);

    return updated;
}

bool edge_task_pair_set_exec_site_by_index(int taskIndex, edge_task_execution_site_t exec_site)
{
    return update_monitor_exec_site_locked(taskIndex, exec_site);
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
    if (slot == NULL || !runtime_registry_slot_owned(slot) || !slot->in_use) {
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
    unsigned deadline_ms, 
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
        deadline_ms,
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


edge_task_creation_result_t CreateEATaskPinnedToCoreEx(
    const char *const TaskName,
    uint8_t Priority,
    eaPort_task_function_t TaskCodeClient,
    eaPort_task_function_t TaskCodeServer,
    const uint32_t MemStackDepthClient,
    const uint32_t MemStackDepthServer,
    const uint8_t CoreID,
    edge_task_type_t AppType,
    unsigned deadline_ms,
    uint8_t DelaySensibility,
    uint8_t EnergySensibility,
    edge_task_execution_site_t DefaultExecutionSite,
    const edge_task_pair_spec_t *pairSpec,
    const char *const HostName,
    uint32_t xPeriod,
    unsigned WCET_c,
    unsigned WCET_s)
{
    edge_task_creation_result_t result = {
        .task_index = -1,
        .failure_reason = EDGE_TASK_CREATION_FAILURE_NONE,
    };
    const edge_task_pair_spec_t *resolvedSpec = pairSpec;
    edge_task_pair_runtime_t *edgeRuntime = NULL;
    int task_index = -1;
    int actualCore = (CoreID == eaPort_NO_AFFINITY) ? 0 : CoreID;

    if (TaskName == NULL || TaskCodeClient == NULL || TaskCodeServer == NULL || resolvedSpec == NULL) {
        result.failure_reason = EDGE_TASK_CREATION_FAILURE_INVALID_SPEC;
        return result;
    }

    if (resolvedSpec->queue_depth == 0U || resolvedSpec->message_size == 0U) {
        result.failure_reason = EDGE_TASK_CREATION_FAILURE_INVALID_SPEC;
        return result;
    }

    edgeRuntime = reserve_pair_runtime();
    if (edgeRuntime == NULL) {
        result.failure_reason = EDGE_TASK_CREATION_FAILURE_RUNTIME_SLOT;
        return result;
    }

    edgeRuntime->client_index = -1;
    edgeRuntime->server_index = -1;
    if (HostName != NULL) {
        snprintf(edgeRuntime->host_name, sizeof(edgeRuntime->host_name), "%s", HostName);
    }
    edgeRuntime->role = EDGE_TASK_PAIR_LOCAL;

    if (edge_task_manager_test_hook_should_fail_creation(EDGE_TASK_CREATION_FAILURE_QUEUE_CLIENT, TaskName)) {
        result.failure_reason = EDGE_TASK_CREATION_FAILURE_QUEUE_CLIENT;
        goto error_cleanup;
    }

    edgeRuntime->queue_client_server = eaPort_Queue_Create(resolvedSpec->queue_depth, resolvedSpec->message_size);
    if (edgeRuntime->queue_client_server == NULL) {
        result.failure_reason = EDGE_TASK_CREATION_FAILURE_QUEUE_CLIENT;
        goto error_cleanup;
    }

    if (edge_task_manager_test_hook_should_fail_creation(EDGE_TASK_CREATION_FAILURE_QUEUE_SERVER, TaskName)) {
        result.failure_reason = EDGE_TASK_CREATION_FAILURE_QUEUE_SERVER;
        goto error_cleanup;
    }

    edgeRuntime->queue_server_client = eaPort_Queue_Create(resolvedSpec->queue_depth, resolvedSpec->message_size);
    if (edgeRuntime->queue_server_client == NULL) {
        result.failure_reason = EDGE_TASK_CREATION_FAILURE_QUEUE_SERVER;
        goto error_cleanup;
    }

    runtime_registry_init();
    eaPort_Mutex_Enter(runtimeRegistryMux);
    edgeRuntime->pair_id = next_pair_id_locked();
    eaPort_Mutex_Exit(runtimeRegistryMux);

    if (AppType != LOCAL) {
        char clientBaseName[CONFIG_EA_MAX_TASK_NAME_LEN];
        char serverBaseName[CONFIG_EA_MAX_TASK_NAME_LEN];

        snprintf(clientBaseName, sizeof(clientBaseName), "%s-cl-%i", TaskName, actualCore);
        snprintf(serverBaseName, sizeof(serverBaseName), "%s-sv-%i", TaskName, actualCore);

        snprintf(edgeRuntime->client_name, sizeof(edgeRuntime->client_name), "%s", clientBaseName);
        snprintf(edgeRuntime->server_name, sizeof(edgeRuntime->server_name), "%s", serverBaseName);
        edgeRuntime->role = EDGE_TASK_PAIR_CLIENT;

        printf("Creation: Creating Edge Task: %s.\n", TaskName);

        if (edge_task_manager_test_hook_should_fail_creation(EDGE_TASK_CREATION_FAILURE_CLIENT_TASK, clientBaseName)) {
            result.failure_reason = EDGE_TASK_CREATION_FAILURE_CLIENT_TASK;
            goto error_cleanup;
        }

        task_index = _CreateTaskPinnedToCore_(
            TaskCodeClient,
            clientBaseName,
            MemStackDepthClient,
            (void *)edgeRuntime,
            Priority,
            CoreID,
            AppType,
            CLIENT_SEGMENT,
            deadline_ms,
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
            result.failure_reason = EDGE_TASK_CREATION_FAILURE_CLIENT_TASK;
            goto error_cleanup;
        }

        edgeRuntime->client_index = task_index;
        update_monitor_linkage_by_index((size_t)task_index, edgeRuntime->pair_id, task_index, edgeRuntime->server_index);

        if (DefaultExecutionSite == LOCAL_EXECUTION) {
            if (edge_task_manager_test_hook_should_fail_creation(EDGE_TASK_CREATION_FAILURE_SERVER_TASK, serverBaseName)) {
                result.failure_reason = EDGE_TASK_CREATION_FAILURE_SERVER_TASK;
                goto error_cleanup;
            }

            task_index = _CreateTaskPinnedToCore_(
                TaskCodeServer,
                serverBaseName,
                MemStackDepthServer,
                (void *)edgeRuntime,
                Priority,
                CoreID,
                AppType,
                SERVER_SEGMENT,
                deadline_ms,
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
                result.failure_reason = EDGE_TASK_CREATION_FAILURE_SERVER_TASK;
                goto error_cleanup;
            }

            edgeRuntime->server_index = task_index;
            update_monitor_linkage_by_index((size_t)edgeRuntime->client_index, edgeRuntime->pair_id, edgeRuntime->client_index, edgeRuntime->server_index);
            update_monitor_linkage_by_index((size_t)edgeRuntime->server_index, edgeRuntime->pair_id, edgeRuntime->server_index, edgeRuntime->client_index);
        }
    } else {
        char localBaseName[CONFIG_EA_MAX_TASK_NAME_LEN];

        snprintf(localBaseName, sizeof(localBaseName), "%s-lc-%i", TaskName, actualCore);
        snprintf(edgeRuntime->client_name, sizeof(edgeRuntime->client_name), "%s", localBaseName);
        edgeRuntime->server_name[0] = '\0';
        edgeRuntime->role = EDGE_TASK_PAIR_LOCAL;

        if (edge_task_manager_test_hook_should_fail_creation(EDGE_TASK_CREATION_FAILURE_LOCAL_TASK, localBaseName)) {
            result.failure_reason = EDGE_TASK_CREATION_FAILURE_LOCAL_TASK;
            goto error_cleanup;
        }

        task_index = _CreateTaskPinnedToCore_(
            TaskCodeClient,
            localBaseName,
            MemStackDepthClient,
            (void *)edgeRuntime,
            Priority,
            CoreID,
            AppType,
            UNIQUE_SEGMENT,
            deadline_ms,
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
            result.failure_reason = EDGE_TASK_CREATION_FAILURE_LOCAL_TASK;
            goto error_cleanup;
        }

        edgeRuntime->client_index = task_index;
        update_monitor_linkage_by_index((size_t)task_index, edgeRuntime->pair_id, task_index, -1);
    }

    result.task_index = edgeRuntime->client_index;
    result.failure_reason = EDGE_TASK_CREATION_FAILURE_NONE;
    return result;

error_cleanup:
    if (edgeRuntime != NULL) {
        runtime_mark_cleaning(edgeRuntime);
        cleanup_task_handle(&edgeRuntime->HandlerServer);
        cleanup_task_handle(&edgeRuntime->HandlerClient);
        cleanup_queue_handle(&edgeRuntime->queue_client_server);
        cleanup_queue_handle(&edgeRuntime->queue_server_client);
        clear_runtime_monitor_entries(edgeRuntime->client_index, edgeRuntime->server_index, edgeRuntime->server_index >= 0);
        release_pair_runtime_impl(edgeRuntime);
    }

    return result;
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
    unsigned deadline_ms,
    uint8_t DelaySensibility,
    uint8_t EnergySensibility,
    edge_task_execution_site_t DefaultExecutionSite,
    const edge_task_pair_spec_t *pairSpec,
    const char *const HostName,
    uint32_t xPeriod,
    unsigned WCET_c,
    unsigned WCET_s)
{
    edge_task_creation_result_t result = CreateEATaskPinnedToCoreEx(
        TaskName,
        Priority,
        TaskCodeClient,
        TaskCodeServer,
        MemStackDepthClient,
        MemStackDepthServer,
        CoreID,
        AppType,
        deadline_ms,
        DelaySensibility,
        EnergySensibility,
        DefaultExecutionSite,
        pairSpec,
        HostName,
        xPeriod,
        WCET_c,
        WCET_s);

    if (result.failure_reason != EDGE_TASK_CREATION_FAILURE_NONE) {
        printf("Error: Task creation failed (%s).\n", edge_task_creation_failure_reason_to_string(result.failure_reason));
        return -1;
    }

    return 0;
}
