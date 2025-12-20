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




typedef enum {
  LOCAL =       0,
  ENHANCED =    1,
  ACCELERATED = 2,
  NATIVE =      3,
  CLOUD =       4
} edge_task_type_t;


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


typedef enum {
  SIGNAL_WAIT    =       0,
  SIGNAL_SUSPEND =       1,
  SIGNAL_SWITCH  =       2
} edge_task_signal_t;


typedef struct {
  uint32_t start_tick;
  uint32_t start_cycles;
} edge_task_monitor_sample_t;



//---------------------------------------------------------------------
// task_monitor_t structure
typedef struct {
  char name[CONFIG_EA_MAX_TASK_NAME_LEN];    // Task name
  uint32_t runTime;                       // Total run-time in microseconds (from vTaskGetRunTimeStats)
  float cpuUsagePercent;                  // CPU usage in percentage (calculated from runTime vs total run time)
  char state;                             // Task state (e.g., 'R', 'B', etc.)
  unsigned priority;                      // Task priority
  unsigned stackHighWaterMark;            // Minimum amount of stack (in words) that has remained for the task (or current usage)
  edge_task_type_t app_type;               // type of application
  edge_task_segment_t app_segment;         // The segment of the application (client or server)
  unsigned MAE2EL;                        // Deadline Max Accepted E2E Latency
  unsigned last_start;                    // Last time the task started in ticks
  unsigned last_end;                      // Last time the task ended in ticks
  unsigned OE2EL;                         // Latest Observed E2E Latency
  unsigned latest_local;                  // Latest Observed E2E Latency when task was local
  unsigned WCET;                          // WCET registered for the task after profiling
  edge_task_execution_site_t ex_site;                  // The current execution site of the task
  uint8_t core;                           // Core index where the task executes
  uint32_t cpu_cycles;                    // Number of Cpu cycles the task took to execute (do not disseminate preemtion time or interrupts)
  uint32_t data_size;                     // The size of the data sent from client-server, server-client
  uint8_t delay_weight;                   // Weight for deadline importance (a in a+b=100)
  uint8_t energy_weight;                  // Weight for energy importance (b in a+b=100)
  eaPort_task_t TaskHandle;                // The scheduler handle for the task
  char relation[CONFIG_EA_MAX_TASK_NAME_LEN];// Related task name (e.g., client/server counterpart)
  eaPort_task_t RelationTaskHandle;        // The scheduler handle for the task
  char host[32];                          // Host associated with the task
  volatile edge_task_signal_t signal_request;     // Flag to send a signal to the task
  unsigned remote_time;                   // Latest Observed E2E Latency
  uint32_t periodTicks;
  uint32_t    lastWindowCycles;
  float       lastWindowTimeMs;
} edge_task_monitor_t;


//---------------------------------------------------------------------
// Structure used to pass communication queues to an EDGE task.
// The client uses queue_client_server to send to the server, and the server uses
// queue_server_client to reply to the client.
typedef struct {
  eaPort_queue_t queue_client_server;  // Client-to-server queue
  eaPort_queue_t queue_server_client;  // Server-to-client queue
  uint32_t  abs_deadline; 
  eaPort_task_t  HandlerServer; 
  eaPort_task_t  HandlerClient; 
} edge_task_params_t;
//---------------------------------------------------------------------




#ifdef __cplusplus
}
#endif

#endif /* TASKMANAGER_H */