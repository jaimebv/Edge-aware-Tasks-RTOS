

// Configurable parameters for CPU load and message sizes (adjust as needed)
// HIGH-COMPUTE HIGH-NETWORK
#define CLIENT_PROCESSING_CYCLES_1      500000    // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_1      50000000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_1           10000     // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_1  4096      // bytes to send back from server
#define TASK1_PERIOD pdMS_TO_TICKS(1000)          // Period of the task
#define TASK1_PRIORITY 2                          // Priority of the task
#define TASK1_ALPHA 10                            // DelaySensibility percentage
#define TASK1_BETA  90                            // EnergySensibility percentage
#define TASK1_WCET_CLIENT   6                     // WCET for client segment      
#define TASK1_WCET_LSERVER  247                   // WCET for local server segment        


// HIGH-COMPUTE LOW-NETWORK
#define CLIENT_PROCESSING_CYCLES_2      300000    // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_2      20000000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_2           1024      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_2  128       // bytes to send back from server
#define TASK2_PERIOD pdMS_TO_TICKS(500)           // Period of the task
#define TASK2_PRIORITY 2                          // Priority of the task
#define TASK2_ALPHA 50                            // DelaySensibility percentage
#define TASK2_BETA  50                            // EnergySensibility percentage
#define TASK2_WCET_CLIENT   4                     // WCET for client segment      
#define TASK2_WCET_LSERVER  125                   // WCET for local server segment       

// MEDIUM-COMPUTE MEDIUM-NETWORK
#define CLIENT_PROCESSING_CYCLES_3      300000    // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_3      10000000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_3           4096      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_3  1024      // bytes to send back from server
#define TASK3_PERIOD pdMS_TO_TICKS(300)           // Period of the task
#define TASK3_PRIORITY 2                          // Priority of the task
#define TASK3_ALPHA 80                            // DelaySensibility percentage
#define TASK3_BETA  20                            // EnergySensibility percentage
#define TASK3_WCET_CLIENT   4                     // WCET for client segment      
#define TASK3_WCET_LSERVER  62                    // WCET for local server segment       



// // HIGH-COMPUTE HIGH-NETWORK
// #define CLIENT_PROCESSING_CYCLES_1      500000  // e.g., number of CPU cycles to busy-wait in client
// #define SERVER_PROCESSING_CYCLES_1      9600000  // e.g., number of CPU cycles to busy-wait in server
// #define CLIENT_MESSAGE_SIZE_1           70656     // bytes to send from client
// #define SERVER_RESPONSE_MESSAGE_SIZE_1  4096      // bytes to send back from server
// #define TASK1_PERIOD pdMS_TO_TICKS(100)           // Period of the task

// // HIGH-COMPUTE LOW-NETWORK
// #define CLIENT_PROCESSING_CYCLES_2      500000    // e.g., number of CPU cycles to busy-wait in client
// #define SERVER_PROCESSING_CYCLES_2      3200000  // e.g., number of CPU cycles to busy-wait in server
// #define CLIENT_MESSAGE_SIZE_2           4096      // bytes to send from client
// #define SERVER_RESPONSE_MESSAGE_SIZE_2  1024      // bytes to send back from server
// #define TASK2_PERIOD pdMS_TO_TICKS(100)           // Period of the task

// // MEDIUM-COMPUTE MEDIUM-NETWORK
// #define CLIENT_PROCESSING_CYCLES_3      500000      // e.g., number of CPU cycles to busy-wait in client
// #define SERVER_PROCESSING_CYCLES_3      3200000  // e.g., number of CPU cycles to busy-wait in server
// #define CLIENT_MESSAGE_SIZE_3           4096      // bytes to send from client
// #define SERVER_RESPONSE_MESSAGE_SIZE_3  1024      // bytes to send back from server
// #define TASK3_PERIOD pdMS_TO_TICKS(100)           // Period of the task














// LOW-COMPUTE HIGH-NETWORK
#define CLIENT_PROCESSING_CYCLES_4      300000  // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_4      5000000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_4           30000      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_4  4096      // bytes to send back from server
#define TASK4_PERIOD pdMS_TO_TICKS(500)           // Period of the task

// LOW-COMPUTE LOW-NETWORK
#define CLIENT_PROCESSING_CYCLES_5      500000  // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_5      5000000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_5           1024      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_5  128      // bytes to send back from server
#define TASK5_PERIOD pdMS_TO_TICKS(200)           // Period of the task




//------------------------------------
#define CLIENT_PROCESSING_CYCLES_6      1500000  // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_6      1500000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_6           512      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_6  256      // bytes to send back from server
#define TASK6_PERIOD pdMS_TO_TICKS(800)           // Period of the task


#define CLIENT_PROCESSING_CYCLES_7      1500000  // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_7      1500000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_7           512      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_7  256      // bytes to send back from server
#define TASK7_PERIOD pdMS_TO_TICKS(1000)           // Period of the task


#define CLIENT_PROCESSING_CYCLES_8      1500000  // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_8      1500000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_8           512      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_8  256      // bytes to send back from server
#define TASK8_PERIOD pdMS_TO_TICKS(80)           // Period of the task


#define CLIENT_PROCESSING_CYCLES_9      1500000  // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_9     1500000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_9           512      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_9  256      // bytes to send back from server
#define TASK9_PERIOD pdMS_TO_TICKS(80)           // Period of the task


#define CLIENT_PROCESSING_CYCLES_10      1500000  // e.g., number of CPU cycles to busy-wait in client
#define SERVER_PROCESSING_CYCLES_10      1500000  // e.g., number of CPU cycles to busy-wait in server
#define CLIENT_MESSAGE_SIZE_10           512      // bytes to send from client
#define SERVER_RESPONSE_MESSAGE_SIZE_10  256      // bytes to send back from server
#define TASK10_PERIOD pdMS_TO_TICKS(80)           // Period of the task
