#ifndef TASKMANAGER_H
#define TASKMANAGER_H


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "config/config_ea_system.h"
#include "port/port_interface_types.h"


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
 * @brief Structure for monitoring edge task metrics.
 * Includes various performance and execution metrics.
 * Used for performance monitoring and decision-making.
 */
// typedef struct {
//   char name[CONFIG_EA_MAX_TASK_NAME_LEN];       // Task name
//   //uint32_t runTime;                             // Total run-time in microseconds
//   //float cpuUsagePercent;                        // CPU usage in percentage (calculated from runTime vs total run time)
//   //char state;                                   // Task state (e.g., 'R', 'B', etc.)
//   uint8_t priority;                            // Task priority
//   //unsigned stackHighWaterMark;                  // Minimum amount of stack (in words) that has remained for the task (or current usage)
//   edge_task_type_t app_type;                    // type of application
//   edge_task_segment_t app_segment;              // The segment of the application (client or server)

//   uint32_t last_start;                          // Last time the task started in ticks
//   uint32_t last_end;                            // Last time the task ended in ticks
//   unsigned MAE2EL;                              // Deadline Max Accepted E2E Latency
//   unsigned OE2EL;                               // Latest Observed E2E Latency
//   unsigned latest_local;                        // Latest Observed E2E Latency when task was local
//   unsigned WCET;                                // WCET registered for the task after profiling
//   edge_task_execution_site_t ex_site;           // The current execution site of the task
//   uint8_t core;                                 // Core index where the task executes
//   uint32_t cpu_cycles;                          // Number of Cpu cycles the task took to execute (do not disseminate preemtion time or interrupts)
//   uint32_t data_size;                           // The size of the data sent from client-server, server-client
//   uint8_t delay_weight;                         // Weight for deadline importance (a in a+b=100)
//   uint8_t energy_weight;                        // Weight for energy importance (b in a+b=100)
//   eaPort_task_t TaskHandle;                     // The scheduler handle for the task
//   char relation[CONFIG_EA_MAX_TASK_NAME_LEN];   // Related task name (e.g., client/server counterpart)
//   eaPort_task_t RelationTaskHandle;             // The scheduler handle for the task
//   char host[32];                                // Host associated with the task
//   volatile edge_task_signal_t signal_request;   // Flag to send a signal to the task
//   unsigned remote_time;                         // Latest Observed E2E Latency
//   uint32_t periodTicks;                         // Task period in ticks
//   uint32_t    lastWindowCycles;                 // Cpu cycles at the start of the last monitoring window
//   float       lastWindowTimeMs;                 // Time in ms at the start of the last monitoring window
// } edge_task_monitor_t;

typedef struct {
    char                    name[CONFIG_EA_MAX_TASK_NAME_LEN];
    uint32_t                cpu_cycles;
    uint32_t                data_size;
    uint32_t                start_tick;
    uint32_t                end_tick;
    uint32_t                period;
    unsigned                OE2EL;
    unsigned                MAE2EL;
    unsigned                WCET;
    volatile edge_task_signal_t signal_request;
    edge_task_execution_site_t exec_site;
    uint8_t                 delay_weight;
    uint8_t                 energy_weight;
    uint8_t                 core;
    bool                    is_active;
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
 * @brief Opaque runtime state shared by the paired client/server tasks.
 *
 * Use the accessor helpers below instead of reaching into the structure
 * directly. This keeps the task runtime separate from monitoring metadata.
 */
typedef struct edge_task_pair_runtime edge_task_pair_runtime_t;

typedef enum {
  EDGE_TASK_PAIR_CLIENT = 0,
  EDGE_TASK_PAIR_SERVER = 1,
  EDGE_TASK_PAIR_LOCAL  = 2,
} edge_task_pair_role_t;


/*===========================================================================*/
/* EXTERNAL VARIABLES                                                        */
/*===========================================================================*/

//extern edge_task_monitor_t monitoredTasks[CONFIG_EA_MAX_TASKS];
//extern size_t numMonitoredTasks;
//extern eaPort_mutex_t monitoredTasksMux;


/**
 * @brief Initialize the task manager (create mutexes, clear arrays)
 * Must be called before creating any tasks.
 */
void task_manager_init(void);


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
);


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
    unsigned WCET_s);

/*===========================================================================*/
/* GET METHODS                                                         */
/*===========================================================================*/

bool get_task_snapshot(const char* taskName, task_snapshot_t* out_snapshot);

size_t get_num_monitored_tasks(void);

eaPort_queue_t edge_task_pair_queue_client_to_server(const edge_task_pair_runtime_t *runtime);

eaPort_queue_t edge_task_pair_queue_server_to_client(const edge_task_pair_runtime_t *runtime);

eaPort_task_t edge_task_pair_client_handle(const edge_task_pair_runtime_t *runtime);

eaPort_task_t edge_task_pair_server_handle(const edge_task_pair_runtime_t *runtime);

const char *edge_task_pair_client_name(const edge_task_pair_runtime_t *runtime);

const char *edge_task_pair_server_name(const edge_task_pair_runtime_t *runtime);

const char *edge_task_pair_host_name(const edge_task_pair_runtime_t *runtime);

edge_task_pair_role_t edge_task_pair_role(const edge_task_pair_runtime_t *runtime);


int get_task_index(const char *taskName);


int get_task_signal(int taskIndex);


const char* get_task_ex_site(const char *taskName);

uint32_t get_task_cpu_cycles(int taskIndex);


uint32_t get_task_data_size(int taskIndex);


unsigned get_task_OE2EL(int taskIndex);


unsigned get_task_WCET(int taskIndex);


/*===========================================================================*/
/* TASK MANAGEMENT METHODS                                                   */
/*===========================================================================*/
edge_task_monitor_sample_t start_task_monitoring(void);


int end_task_monitoring(
    edge_task_monitor_sample_t sample,
    uint32_t message_size);


int end_task_monitoring_(
    edge_task_monitor_sample_t sample,
    uint32_t message_size,
    uint32_t remote_time, 
    bool remote_flag);


void check_self_suspend (int taskIndex);


void update_task_metrics(
    const char *taskName, 
    uint32_t newOE2EL, 
    uint32_t newCycles, 
    uint32_t newStartTick,
    uint32_t newEndTick, 
    uint32_t newDataSize);


void update_task_metrics_by_index(
    int taskIndex, 
    uint32_t newOE2EL, 
    uint32_t newCycles, 
    uint32_t newStartTick,
    uint32_t newEndTick, 
    uint32_t newDataSize);


void update_task_metrics_OE2EL(const char *taskName, uint32_t newOE2EL);


int find_task_index(const char *taskName);


void remove_monitored_task(const char *taskName);


int is_client_task(const char* str);


uint32_t tasks_compute_hyperperiod();





#ifdef __cplusplus
}
#endif

#endif /* TASKMANAGER_H */
