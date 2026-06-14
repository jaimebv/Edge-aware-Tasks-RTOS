/**
 * @file task_manager.h
 * @brief Task management and monitoring API for EEAA.
 *
 * @details
 * This module owns the public task lifecycle API, the runtime accessors used
 * by paired tasks, and the monitoring helpers that expose task snapshots and
 * metrics to the rest of the system.
 */

#ifndef TASKMANAGER_H
#define TASKMANAGER_H


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "config/config_ea_system.h"
#include "port/port_interface_types.h"


/** @defgroup eeaa_task_manager Task manager API
 * @brief Task lifecycle, runtime accessors, snapshots, and cleanup helpers.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Enumeration of edge task types.
 * Used for categorizing tasks in the edge-aware RTOS.
 * LOCAL: Task only runs locally.
 * ENRICHED: Task with enhanced capabilities when runs at the edge, thus can be offloaded.
 * REMOTE: Task designed to always run remotely (e.g., remote edge or in the cloud).
 */
typedef enum {
  LOCAL =       0,
  ENRICHED =    1,
  REMOTE =      2,
} edge_task_type_t;


/**
 * @brief Enumeration of edge task segments.
 * Used for categorizing tasks based on their segment in the edge-aware RTOS.
 * CLIENT_SEGMENT: Task belongs to the client segment.
 * SERVER_SEGMENT: Task belongs to the server segment.
 * UNIQUE_SEGMENT: Task is unique and does not belong to client or server segments.
 */
typedef enum {
  CLIENT_SEGMENT  =      0,
  SERVER_SEGMENT =       1,
  UNIQUE_SEGMENT =       2
} edge_task_segment_t;


/**
 * @brief Enumeration of edge task execution sites.
 * Used to indicate where the task is currently executing.
 * LOCAL_EXECUTION: Task is executing locally.
 * REMOTE_EXECUTION: Task is executing remotely (e.g., on an edge server or
 * cloud).
 */
typedef enum {
  LOCAL_EXECUTION  =       0,
  REMOTE_EXECUTION =       1
} edge_task_execution_site_t;


/**
 * @brief Enumeration of signals that can be sent to edge tasks.
 * SIGNAL_WAIT: Task should wait.
 * SIGNAL_SUSPEND: Task should suspend its execution.
 * SIGNAL_SWITCH: Task should switch its execution site.
 */
typedef enum {
  SIGNAL_WAIT    =       0,
  SIGNAL_SUSPEND =       1,
  SIGNAL_SWITCH  =       2
} edge_task_signal_t;


/**
 * @brief Structure for sampling task monitoring data.
 */
typedef struct {
  uint32_t start_tick;
  uint32_t start_cycles;
} edge_task_monitor_sample_t;


/**
 * @brief Hot monitoring state updated on the execution path.
 * Keep this small and fast.
 */
typedef struct {
    uint32_t                pair_id;
    int32_t                 task_index;
    int32_t                 peer_index;
    uint32_t                cpu_cycles;
    uint32_t                data_size;
    uint32_t                start_tick;
    uint32_t                end_tick;
    unsigned                OE2EL;
    volatile edge_task_signal_t signal_request;
    uint8_t                 core;
    bool                    is_active;
} edge_task_monitor_hot_t;

/**
 * @brief Cold monitoring metadata updated by the controller/offloader path.
 * Keep identity-linkage in hot state; cold state is for names, host,
 * execution-site, scheduling targets, and debug metadata.
 */
typedef struct {
    char                    name[CONFIG_EA_MAX_TASK_NAME_LEN];
    char                    host[CONFIG_EA_MAX_TASK_NAME_LEN];
    uint32_t                pair_id;
    int32_t                 task_index;
    int32_t                 peer_index;
    uint32_t                period;
    unsigned                deadline_ms;
    unsigned                WCET;
    edge_task_execution_site_t exec_site;
    uint8_t                 delay_weight;
    uint8_t                 energy_weight;
} edge_task_monitor_cold_t;

/**
 * @brief Backwards-compatible aggregate record.
 * Prefer the hot/cold split above for new code.
 */
typedef struct {
    edge_task_monitor_hot_t  hot;
    edge_task_monitor_cold_t cold;
} edge_task_monitor_t;


/**
 * @brief Structure for taking a snapshot of a task's state.
 */
typedef struct {
    char          name[CONFIG_EA_MAX_TASK_NAME_LEN];
    uint32_t      cpu_cycles;
    uint32_t      period;
    unsigned      WCET;
    unsigned      OE2EL;
    bool          valid;
} task_snapshot_t;


/**
 * @brief Immutable configuration for a paired edge task.
 *
 * The caller must provide the queue contract explicitly.
 * The spec is consumed during creation time to size the queues used by the
 * client/server pair.
 */
typedef struct {
  uint32_t queue_depth;
  uint32_t message_size;
} edge_task_pair_spec_t;

/**
 * @brief Public task declaration for the FreeRTOS edge-enrichment runtime.
 *
 * The spec is intentionally explicit so application code can describe the
 * work without touching runtime internals. It is a thin, stable view over the
 * current task manager creation contract.
 */
typedef struct {
  const char *task_name;
  uint8_t priority;
  eaPort_task_function_t client_task_code;
  eaPort_task_function_t server_task_code;
  uint32_t client_stack_depth;
  uint32_t server_stack_depth;
  uint8_t core_id;
  edge_task_type_t app_type;
  edge_task_execution_site_t default_execution_site;
  edge_task_pair_spec_t pair_spec;
  const char *host_name;
  uint32_t period_ms;
  unsigned deadline_ms;
  uint8_t delay_weight;
  uint8_t energy_weight;
  unsigned client_wcet;
  unsigned server_wcet;
} edge_task_spec_t;


/**
 * @brief Opaque runtime state shared by the paired client/server tasks.
 *
 * Owned by the task manager. Callers should treat pointers as borrowed views
 * only and must not free or persist them after the runtime is retired.
 */
typedef struct edge_task_pair_runtime edge_task_pair_runtime_t;

/**
 * @brief API role for paired task runtimes.
 *
 * CLIENT: client half of a pair.
 * SERVER: server half of a pair.
 * LOCAL: single-task mode with no server half.
 */
typedef enum {
  EDGE_TASK_PAIR_CLIENT = 0,
  EDGE_TASK_PAIR_SERVER = 1,
  EDGE_TASK_PAIR_LOCAL  = 2,
} edge_task_pair_role_t;

/**
 * @brief Teardown semantics for runtime cleanup.
 *
 * CLIENT_ONLY removes the client/local task and its monitor entry.
 * PAIR removes both halves, queues, monitor entries, and releases runtime.
 */
typedef enum {
  EDGE_TASK_CLEANUP_CLIENT_ONLY = 0,
  EDGE_TASK_CLEANUP_PAIR        = 1,
} edge_task_cleanup_mode_t;

typedef enum {
  EDGE_TASK_CREATION_FAILURE_NONE = 0,
  EDGE_TASK_CREATION_FAILURE_INVALID_SPEC,
  EDGE_TASK_CREATION_FAILURE_RUNTIME_SLOT,
  EDGE_TASK_CREATION_FAILURE_QUEUE_CLIENT,
  EDGE_TASK_CREATION_FAILURE_QUEUE_SERVER,
  EDGE_TASK_CREATION_FAILURE_CLIENT_TASK,
  EDGE_TASK_CREATION_FAILURE_SERVER_TASK,
  EDGE_TASK_CREATION_FAILURE_LOCAL_TASK,
} edge_task_creation_failure_reason_t;

typedef struct {
  int task_index;
  edge_task_creation_failure_reason_t failure_reason;
} edge_task_creation_result_t;


/*===========================================================================*/
/* EXTERNAL VARIABLES                                                        */
/*===========================================================================*/

//extern edge_task_monitor_t monitoredTasks[CONFIG_EA_MAX_TASKS];
//extern size_t numMonitoredTasks;
//extern eaPort_mutex_t monitoredTasksMux;


/**
 * @brief Initialize the task manager.
 *
 * Creates the internal mutexes, clears monitoring state, and resets the
 * runtime registry. Call once before creating any tasks.
 */
void task_manager_init(void);

struct edge_task_pair_runtime;
/**
 * @brief Create a task and register its monitoring state.
 *
 * Internal helper used by the task-manager creation flow to create one task,
 * populate its monitor slot, and attach it to the shared runtime record.
 */
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
    struct edge_task_pair_runtime *runtime,
    edge_task_execution_site_t pcExec,
    eaPort_task_t *outTaskHandle,
    uint32_t xPeriod,
    unsigned WCET
);
/**
 * @brief Create a task pair using the legacy integer return contract.
 *
 * This wrapper preserves the historical API behavior and returns 0 on
 * success, -1 on failure.
 */
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
    unsigned WCET_s);

/**
 * @brief Create a task pair and return a structured result.
 *
 * The structured result distinguishes the task index from the precise failure
 * reason, allowing callers and tests to validate rollback behavior.
 */
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
    unsigned WCET_s);

/**
 * @brief Initialize a public task specification with safe defaults.
 *
 * The initializer keeps the public task model easy to adopt while still
 * requiring the caller to fill in the execution details explicitly.
 */
void edge_task_spec_init(edge_task_spec_t *spec);

/**
 * @brief Validate a public task specification.
 *
 * @param[in] spec Public task specification to inspect.
 * @return true when the specification is structurally valid.
 */
bool edge_task_spec_validate(const edge_task_spec_t *spec);

/**
 * @brief Create an edge-aware task from a public task specification.
 *
 * This is the v1 developer-facing entry point for task declaration.
 */
edge_task_creation_result_t CreateEATaskFromSpecEx(const edge_task_spec_t *spec);

/**
 * @brief Legacy integer wrapper for public task specification creation.
 *
 * @return 0 on success, -1 on failure.
 */
int CreateEATaskFromSpec(const edge_task_spec_t *spec);
/**
 * @brief Convert a creation failure reason to a stable text label.
 *
 * @param[in] reason Failure enum value.
 * @return Human-readable static string for logging and tests.
 */
const char *edge_task_creation_failure_reason_to_string(edge_task_creation_failure_reason_t reason);

/*===========================================================================*/
/* GET METHODS                                                               */
/* Legacy name-based wrappers are compatibility-only and should be treated   */
/* as deprecated in new code. Prefer index-based helpers where possible.     */
/* Runtime accessors are valid only while the runtime is active.             */
/* NULL-safe behavior: inactive or NULL inputs return NULL/0/false.          */
/*===========================================================================*/

/**
 * @deprecated Compatibility wrapper for legacy name-based snapshot lookup.
 * Prefer get_task_snapshot_by_index() when the task index is known.
 */
bool get_task_snapshot(const char* taskName, task_snapshot_t* out_snapshot);

/**
 * @brief Snapshot a monitored task by index.
 *
 * Valid only while the task is active; returns false for invalid, destroyed,
 * or inactive entries.
 */
bool get_task_snapshot_by_index(int taskIndex, task_snapshot_t* out_snapshot);

/**
 * @brief Return the number of active monitored tasks.
 *
 * @return Number of currently active monitor entries.
 */
size_t get_num_monitored_tasks(void);

/**
 * @brief Get the client->server queue for an active runtime.
 * Returns NULL if the runtime is NULL or inactive.
 */
eaPort_queue_t edge_task_pair_queue_client_to_server(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the server->client queue for an active runtime.
 * Returns NULL if the runtime is NULL or inactive.
 */
eaPort_queue_t edge_task_pair_queue_server_to_client(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the client task handle for an active runtime.
 * Returns NULL if the runtime is NULL or inactive.
 */
eaPort_task_t edge_task_pair_client_handle(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the server task handle for an active runtime.
 * Returns NULL if the runtime is NULL or inactive.
 */
eaPort_task_t edge_task_pair_server_handle(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the client task name for an active runtime.
 * Returns NULL if the runtime is NULL or inactive.
 */
const char *edge_task_pair_client_name(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the server task name for an active runtime.
 * Returns NULL if the runtime is NULL or inactive.
 */
const char *edge_task_pair_server_name(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the host label for an active runtime.
 * Returns NULL if the runtime is NULL or inactive.
 */
const char *edge_task_pair_host_name(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the runtime role.
 * Returns EDGE_TASK_PAIR_LOCAL for NULL or inactive runtimes.
 */
edge_task_pair_role_t edge_task_pair_role(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the pair identifier.
 * Returns 0 for NULL or inactive runtimes.
 */
uint32_t edge_task_pair_id(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the client task index.
 * Returns -1 for NULL or inactive runtimes.
 */
int edge_task_pair_task_index(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the peer task index.
 * Returns -1 for NULL or inactive runtimes.
 */
int edge_task_pair_peer_index(const edge_task_pair_runtime_t *runtime);

/**
 * @brief Get the borrowed runtime pointer for a monitored task index.
 *
 * Returns NULL when the index is invalid or inactive. The pointer is owned by
 * the task manager and remains valid only while the runtime is active.
 */
const edge_task_pair_runtime_t *edge_task_pair_runtime_by_task_index(int taskIndex);

/**
 * @brief Release a runtime pointer owned by the task manager.
 *
 * Only valid while the runtime is still active or during teardown code that
 * is explicitly retiring the allocation.
 */
void edge_task_pair_runtime_release(edge_task_pair_runtime_t *runtime);

/**
 * @brief Tear down a task runtime in a portable way.
 *
 * EDGE_TASK_CLEANUP_CLIENT_ONLY removes the client task and its monitor entry.
 * EDGE_TASK_CLEANUP_PAIR removes both tasks, queues, monitor entries, and
 * releases the runtime slot back to the framework.
 */
int edge_task_pair_destroy(edge_task_pair_runtime_t *runtime, edge_task_cleanup_mode_t mode);

/**
 * @deprecated Compatibility wrapper for legacy index-based teardown.
 * Prefer edge_task_pair_destroy() when a runtime pointer is available.
 */
int edge_task_pair_destroy_by_task_index(int taskIndex, edge_task_cleanup_mode_t mode);

/**
 * @deprecated Compatibility wrapper for legacy name-based teardown.
 * Prefer index-based teardown when the runtime index is available.
 */
int edge_task_pair_destroy_by_name(const char *taskName, edge_task_cleanup_mode_t mode);


/**
 * @deprecated Compatibility wrapper for legacy name-based lookup.
 * Prefer runtime accessors or tracked task indices in new code.
 */
int get_task_index(const char *taskName);


/**
 * @brief Return the current signal request for a monitored task.
 *
 * @param[in] taskIndex Monitored task index.
 * @return Current signal value, or -1 when the index is invalid.
 */
int get_task_signal(int taskIndex);


/**
 * @deprecated Compatibility wrapper for legacy name-based lookup.
 * Prefer get_task_ex_site_by_index() for known monitored indices.
 */
const char* get_task_ex_site(const char *taskName);

/**
 * @brief Return the execution-site label for a monitored task.
 *
 * @param[in] taskIndex Monitored task index.
 * @return Static execution-site string, or "UNKNOWN" if invalid.
 */
const char* get_task_ex_site_by_index(int taskIndex);

/**
 * @brief Return the accumulated CPU cycles for a monitored task.
 *
 * @param[in] taskIndex Monitored task index.
 * @return CPU cycle count, or 0 when the index is invalid.
 */
uint32_t get_task_cpu_cycles(int taskIndex);


/**
 * @brief Return the last recorded data size for a monitored task.
 *
 * @param[in] taskIndex Monitored task index.
 * @return Recorded payload size in bytes, or 0 when invalid.
 */
uint32_t get_task_data_size(int taskIndex);


/**
 * @brief Return the current OE2EL value for a monitored task.
 *
 * @param[in] taskIndex Monitored task index.
 * @return OE2EL metric, or 0 when invalid.
 */
unsigned get_task_OE2EL(int taskIndex);


/**
 * @brief Return the WCET configured for a monitored task.
 *
 * @param[in] taskIndex Monitored task index.
 * @return WCET value, or 0 when invalid.
 */
unsigned get_task_WCET(int taskIndex);

/**
 * @brief Update the host label for a monitored task and its runtime record.
 *
 * @param[in] taskIndex Monitored task index.
 * @param[in] host Null-terminated host label.
 * @return true when the task is active and the host was updated.
 */
bool edge_task_pair_set_host_by_index(int taskIndex, const char *host);

/**
 * @brief Update the execution site for a monitored task.
 *
 * @param[in] taskIndex Monitored task index.
 * @param[in] exec_site New execution site.
 * @return true when the task is active and the execution site was updated.
 */
bool edge_task_pair_set_exec_site_by_index(int taskIndex, edge_task_execution_site_t exec_site);


/*===========================================================================*/
/* TASK MANAGEMENT METHODS                                                   */
/*===========================================================================*/
/**
 * @brief Capture the start point for a monitoring interval.
 *
 * @return Snapshot containing the current tick and cycle counter.
 */
edge_task_monitor_sample_t start_task_monitoring(void);


/**
 * @brief Finalize monitoring for a local operation.
 *
 * @param[in] sample Monitoring sample previously returned by
 *                   start_task_monitoring().
 * @param[in] message_size Message payload size in bytes.
 * @return Synthetic monitoring cost used by the caller.
 */
int end_task_monitoring(
    edge_task_monitor_sample_t sample,
    uint32_t message_size);


/**
 * @brief Finalize monitoring for a remote operation.
 *
 * @param[in] sample Monitoring sample previously returned by
 *                   start_task_monitoring().
 * @param[in] message_size Message payload size in bytes.
 * @param[in] remote_time Remote execution time contribution.
 * @param[in] remote_flag Whether the remote path was taken.
 * @return Synthetic monitoring cost used by the caller.
 */
int end_task_monitoring_(
    edge_task_monitor_sample_t sample,
    uint32_t message_size,
    uint32_t remote_time, 
    bool remote_flag);

/**
 * @brief Suspend the current task if its monitor requests suspension.
 *
 * @param[in] taskIndex Monitored task index to inspect.
 */
void check_self_suspend (int taskIndex);


/**
 * @deprecated Compatibility wrapper for legacy name-based metric updates.
 * Prefer the index-based update helpers in new code.
 */
void update_task_metrics(
    const char *taskName, 
    uint32_t newOE2EL, 
    uint32_t newCycles, 
    uint32_t newStartTick,
    uint32_t newEndTick, 
    uint32_t newDataSize);


/**
 * @brief Update all hot metrics for a monitored task by index.
 *
 * @param[in] taskIndex Monitored task index.
 * @param[in] newOE2EL New OE2EL metric.
 * @param[in] newCycles New CPU cycle count.
 * @param[in] newStartTick New monitoring start tick.
 * @param[in] newEndTick New monitoring end tick.
 * @param[in] newDataSize New payload size in bytes.
 */
void update_task_metrics_by_index(
    int taskIndex, 
    uint32_t newOE2EL, 
    uint32_t newCycles, 
    uint32_t newStartTick,
    uint32_t newEndTick, 
    uint32_t newDataSize);


/**
 * @deprecated Compatibility wrapper for legacy name-based metric updates.
 * Prefer the index-based update helpers in new code.
 */
void update_task_metrics_OE2EL(const char *taskName, uint32_t newOE2EL);

/**
 * @brief Update only the OE2EL metric for a monitored task.
 *
 * @param[in] taskIndex Monitored task index.
 * @param[in] newOE2EL New OE2EL metric.
 */
void update_task_metrics_OE2EL_by_index(int taskIndex, uint32_t newOE2EL);


/**
 * @deprecated Compatibility wrapper for legacy name-based lookup.
 * Prefer tracked task indices and runtime accessors.
 */
/**
 * @brief Find the monitored index for a task name.
 *
 * @param[in] taskName Null-terminated task name.
 * @return Monitored index, or -1 if the task is not active.
 */
int find_task_index(const char *taskName);


/**
 * @deprecated Compatibility wrapper for legacy name-based removal.
 * Prefer remove_monitored_task_by_index() when the index is known.
 */
void remove_monitored_task(const char *taskName);

/**
 * @brief Remove a monitored task entry by index.
 *
 * @param[in] taskIndex Monitored task index.
 */
void remove_monitored_task_by_index(int taskIndex);


/**
 * @brief Check whether a task name uses the client suffix.
 *
 * @param[in] str Task name to inspect.
 * @return 1 when the name contains the client marker, otherwise 0.
 */
int is_client_task(const char* str);


/**
 * @brief Compute the monitoring hyperperiod.
 *
 * @return Hyperperiod in ticks or 0 when no scheduling model is available.
 */
uint32_t tasks_compute_hyperperiod();





#ifdef __cplusplus
}
#endif

/** @} */

#endif /* TASKMANAGER_H */
