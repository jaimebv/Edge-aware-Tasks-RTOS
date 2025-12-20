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



//---------------------------------------------------------------------
typedef enum {
  LOCAL =       0,
  ENHANCED =    1,
  ACCELERATED = 2,
  NATIVE =      3,
  CLOUD =       4
} edge_task_type_t;

//---------------------------------------------------------------------
typedef enum {
  CLIENT_SEGMENT  =      0,
  SERVER_SEGMENT =       1,
  UNIQUE_SEGMENT =       2
} edge_task_segment_t;

//---------------------------------------------------------------------
typedef enum {
  LOCAL_EXECUTION  =       0,
  REMOTE_EXECUTION =       1
} edge_task_execution_site_t;

//---------------------------------------------------------------------
typedef enum {
  SIGNAL_WAIT    =       0,
  SIGNAL_SUSPEND =       1,
  SIGNAL_SWITCH  =       2
} edge_task_signal_t;

//---------------------------------------------------------------------
typedef struct {
  uint32_t start_tick;
  uint32_t start_cycles;
} edge_task_monitor_sample_t;

//---------------------------------------------------------------------
typedef struct {
  char name[CONFIG_EA_MAX_TASK_NAME_LEN];       // Task name
  uint32_t runTime;                             // Total run-time in microseconds (from vTaskGetRunTimeStats)
  float cpuUsagePercent;                        // CPU usage in percentage (calculated from runTime vs total run time)
  char state;                                   // Task state (e.g., 'R', 'B', etc.)
  unsigned priority;                            // Task priority
  unsigned stackHighWaterMark;                  // Minimum amount of stack (in words) that has remained for the task (or current usage)
  edge_task_type_t app_type;                    // type of application
  edge_task_segment_t app_segment;              // The segment of the application (client or server)
  unsigned MAE2EL;                              // Deadline Max Accepted E2E Latency
  unsigned last_start;                          // Last time the task started in ticks
  unsigned last_end;                            // Last time the task ended in ticks
  unsigned OE2EL;                               // Latest Observed E2E Latency
  unsigned latest_local;                        // Latest Observed E2E Latency when task was local
  unsigned WCET;                                // WCET registered for the task after profiling
  edge_task_execution_site_t ex_site;           // The current execution site of the task
  uint8_t core;                                 // Core index where the task executes
  uint32_t cpu_cycles;                          // Number of Cpu cycles the task took to execute (do not disseminate preemtion time or interrupts)
  uint32_t data_size;                           // The size of the data sent from client-server, server-client
  uint8_t delay_weight;                         // Weight for deadline importance (a in a+b=100)
  uint8_t energy_weight;                        // Weight for energy importance (b in a+b=100)
  eaPort_task_t TaskHandle;                     // The scheduler handle for the task
  char relation[CONFIG_EA_MAX_TASK_NAME_LEN];   // Related task name (e.g., client/server counterpart)
  eaPort_task_t RelationTaskHandle;             // The scheduler handle for the task
  char host[32];                                // Host associated with the task
  volatile edge_task_signal_t signal_request;   // Flag to send a signal to the task
  unsigned remote_time;                         // Latest Observed E2E Latency
  uint32_t periodTicks;                         // Task period in ticks
  uint32_t    lastWindowCycles;                 // Cpu cycles at the start of the last monitoring window
  float       lastWindowTimeMs;                 // Time in ms at the start of the last monitoring window
} edge_task_monitor_t;


//---------------------------------------------------------------------
// Structure used to pass communication queues to an edge task.
// The client uses queue_client_server to send to the server, and the server uses
// queue_server_client to reply to the client.
typedef struct {
  eaPort_queue_t queue_client_server;           // Client-to-server queue
  eaPort_queue_t queue_server_client;           // Server-to-client queue
  uint32_t  abs_deadline;                       // Absolute deadline in ms
  eaPort_task_t  HandlerServer;                 // Server task handle
  eaPort_task_t  HandlerClient;                 // Client task handle
} edge_task_params_t;
//---------------------------------------------------------------------

extern edge_task_monitor_t monitoredTasks[CONFIG_EA_MAX_TASKS];
extern size_t numMonitoredTasks;
extern eaPort_mutex_t monitoredTasksMux;

//---------------------------------------------------------------------

size_t get_num_monitored_tasks(void);


int get_task_Index(const char *taskName);


int get_task_signal(int taskIndex);


const char* get_task_ex_site(const char *taskName);


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


void update_task_metrics_index(
    int taskIndex, 
    uint32_t newOE2EL, 
    uint32_t newCycles, 
    uint32_t newStartTick,
    uint32_t newEndTick, 
    uint32_t newDataSize);


void update_task_metrics_OE2EL(const char *taskName, uint32_t newOE2EL);


int find_task_index(const char *taskName);


eaPort_task_t get_task_handler(const char *taskName);


void remove_monitored_task(const char *taskName);


int is_client_task(const char* str);


uint32_t get_task_cpu_cycles(int taskIndex);


uint32_t get_task_data_size(int taskIndex);


unsigned get_task_OE2EL(int taskIndex);


unsigned get_task_WCET(int taskIndex);


uint32_t tasks_compute_hyperperiod();


int CreateEdgeTaskPinnedToCore(
  eaPort_task_function_t pxTaskCode, 
  const char *const pcName, 
  const uint32_t usStackDepth, 
  void *const pvParameters, 
  uint32_t uxPriority, 
  const int32_t xCoreID, 
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
);


int CreateEdgeTaskPinnedToCoreDynamic(
  const char *const TaskName, 
  uint32_t Priority, 
  eaPort_task_function_t TaskCodeClient, 
  eaPort_task_function_t TaskCodeServer, 
  const uint32_t MemStackDepthClient, 
  const uint32_t MemStackDepthServer, 
  const int32_t CoreID, 
  edge_task_type_t AppType, 
  unsigned MAE2EL, 
  uint8_t DelaySensibility, 
  uint8_t EnergySensibility, 
  edge_task_execution_site_t DefaultExecutionSite, 
  const char *const HostName,
  uint32_t xPeriod,
  unsigned WCET_c, 
  unsigned WCET_s
);


#ifdef __cplusplus
}
#endif

#endif /* TASKMANAGER_H */