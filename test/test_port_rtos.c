/*
 * Port-RTOS regression test harness.
 *
 * This file exercises the EEAA RTOS abstraction layer without depending on
 * FreeRTOS-specific symbols. The goal is to validate the portable contract:
 * task creation, lookup, info queries, queues, mutexes, delays, and teardown.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "port/port_rtos.h"
#include "test_helpers.h"

#define TEST_WORKER_NAME      "RTOS-Worker"
#define TEST_STACK_WORDS      2048U
#define TEST_PRIORITY         3U
#define TEST_QUEUE_DEPTH      4U
#define TEST_DELAY_MS         25U
#define TEST_WAIT_MS          2000U
#define TEST_TASK_LOOKUP_WAIT  20U

enum {
    TEST_CMD_INC = 1,
    TEST_CMD_STOP = 2,
};

typedef struct {
    eaPort_queue_t command_queue;
    eaPort_queue_t reply_queue;
    eaPort_mutex_t mutex;
    volatile bool started;
    volatile bool stop_seen;
    volatile int shared_counter;
    volatile eaPort_task_t task_handle;
} rtos_worker_context_t;

static bool wait_until_true(volatile bool *flag, uint32_t timeout_ms)
{
    uint32_t waited_ms = 0U;

    while (!*flag && waited_ms < timeout_ms) {
        eaPort_Delay_Milliseconds(TEST_TASK_LOOKUP_WAIT);
        waited_ms += TEST_TASK_LOOKUP_WAIT;
    }

    return *flag;
}

static void rtos_worker_task(void *pvParameters)
{
    rtos_worker_context_t *ctx = (rtos_worker_context_t *)pvParameters;
    int command = 0;
    int reply = 0;

    if (ctx == NULL) {
        fail("worker startup", "missing context");
        eaPort_Task_Delete(NULL);
        return;
    }

    ctx->task_handle = eaPort_Get_Current_Task_Handle();
    ctx->started = true;

    while (1) {
        if (eaPort_Queue_Receive(ctx->command_queue, &command, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
            fail("worker command receive", "queue receive failed");
            eaPort_Delay_Milliseconds(TEST_TASK_LOOKUP_WAIT);
            continue;
        }

        if (command == TEST_CMD_INC) {
            eaPort_Mutex_Enter(ctx->mutex);
            ctx->shared_counter += 1;
            reply = ctx->shared_counter;
            eaPort_Mutex_Exit(ctx->mutex);

            if (eaPort_Queue_Send(ctx->reply_queue, &reply, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
                fail("worker reply send", "queue send failed");
            }
        } else if (command == TEST_CMD_STOP) {
            ctx->stop_seen = true;
            break;
        } else {
            fail("worker command dispatch", "unexpected command");
        }
    }

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void test_null_safe_guards(void)
{
    int value = 123;
    int buffer = 0;

    expect_true("queue send null safe", eaPort_Queue_Send(NULL, &value, 0U) == eaPort_STATUS_ERROR, "queue send should fail on NULL queue");
    expect_true("queue receive null safe", eaPort_Queue_Receive(NULL, &buffer, 0U) == eaPort_STATUS_ERROR, "queue receive should fail on NULL queue");
    expect_true("queue waiting null safe", eaPort_Queue_Messages_Waiting(NULL) == 0U, "queue waiting should be zero for NULL");
    expect_true("task lookup null safe", eaPort_Get_Task_Handle_By_Name(NULL) == NULL, "task lookup should fail on NULL name");
    eaPort_Mutex_Destroy(NULL);
    pass("port RTOS null-safe guards");
}

static void test_task_info_null_guards(void)
{
    eaPort_task_info_t info;
    eaPort_task_t task_handle = NULL;

    memset(&info, 0xA5, sizeof(info));
    expect_true("task info null handle", eaPort_Get_Task_Info(&info, &task_handle) == eaPort_STATUS_ERROR,
                "task info query should fail when the handle is NULL");
    expect_true("task info null handle zeroed", info.xTaskHandle == NULL && info.pcTaskName == NULL,
                "task info should be cleared when the handle is NULL");

    memset(&info, 0xA5, sizeof(info));
    expect_true("task info null storage", eaPort_Get_Task_Info(NULL, &task_handle) == eaPort_STATUS_ERROR,
                "task info query should fail when the output storage is NULL");
    pass("port RTOS task info null guards");
}

static void test_time_and_memory_helpers(void)
{
    void *block = eaPort_Malloc(32U);
    eaPort_tick_t start_tick;
    eaPort_tick_t end_tick;
    eaPort_tick_t wake_tick;

    expect_true("memory allocation", block != NULL, "eaPort_Malloc should return a block");
    if (block != NULL) {
        memset(block, 0xA5, 32U);
        eaPort_Free(block);
    }

    start_tick = eaPort_Get_Tick_Time();
    eaPort_Delay_Milliseconds(TEST_DELAY_MS);
    end_tick = eaPort_Get_Tick_Time();
    expect_true("tick monotonic", end_tick >= start_tick, "tick time must not move backwards");

    wake_tick = eaPort_Get_Tick_Time();
    eaPort_Delay_Until(&wake_tick, TEST_DELAY_MS);
    expect_true("delay until advanced wake tick", wake_tick >= end_tick, "delay-until should advance the wake tick");

    {
        eaPort_task_t self = eaPort_Get_Current_Task_Handle();
        eaPort_task_info_t info = {0};
        eaPort_task_t lookup = self;

        expect_true("current task handle", self != NULL, "current task handle should be valid");
        expect_true("current task info", eaPort_Get_Task_Info(&info, &lookup) == eaPort_STATUS_OK, "current task info should be readable");
        expect_true("current task name", info.pcTaskName != NULL && info.pcTaskName[0] != '\0', "current task name should be present");
        expect_true("current task handle roundtrip", info.xTaskHandle == self, "info handle should match current handle");
        expect_true("current task state string", eaPort_Get_Task_State_str(&info) != NULL && eaPort_Get_Task_State_str(&info)[0] != '\0', "current task should have a readable state");
        expect_true("current task state char", strchr("REBSDU", eaPort_Get_Task_State_char(&info)) != NULL,
                    "current task should have a readable state char");
    }

    pass("port RTOS timing and memory helpers");
}

static void test_state_translation_helpers(void)
{
    eaPort_task_info_t info = {0};

    info.eCurrentState = eaPort_TaskState_Running;
    expect_true("state translation running", strcmp(eaPort_Get_Task_State_str(&info), "Running") == 0 && eaPort_Get_Task_State_char(&info) == 'R', "running state translation mismatch");

    info.eCurrentState = eaPort_TaskState_Ready;
    expect_true("state translation ready", strcmp(eaPort_Get_Task_State_str(&info), "Ready") == 0 && eaPort_Get_Task_State_char(&info) == 'E', "ready state translation mismatch");

    info.eCurrentState = eaPort_TaskState_Blocked;
    expect_true("state translation blocked", strcmp(eaPort_Get_Task_State_str(&info), "Blocked") == 0 && eaPort_Get_Task_State_char(&info) == 'B', "blocked state translation mismatch");

    info.eCurrentState = eaPort_TaskState_Suspended;
    expect_true("state translation suspended", strcmp(eaPort_Get_Task_State_str(&info), "Suspended") == 0 && eaPort_Get_Task_State_char(&info) == 'S', "suspended state translation mismatch");

    info.eCurrentState = eaPort_TaskState_Deleted;
    expect_true("state translation deleted", strcmp(eaPort_Get_Task_State_str(&info), "Deleted") == 0 && eaPort_Get_Task_State_char(&info) == 'D', "deleted state translation mismatch");

    info.eCurrentState = eaPort_TaskState_Unknown;
    expect_true("state translation unknown", strcmp(eaPort_Get_Task_State_str(&info), "Unknown") == 0 && eaPort_Get_Task_State_char(&info) == 'U', "unknown state translation mismatch");

    pass("port RTOS state translation helpers");
}

static void send_command(eaPort_queue_t queue, int command, const char *name)
{
    expect_true(name, eaPort_Queue_Send(queue, &command, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK, "command send failed");
}

static int receive_reply(eaPort_queue_t queue, const char *name)
{
    int reply = 0;

    expect_true(name, eaPort_Queue_Receive(queue, &reply, eaPort_WAIT_FOREVER) == eaPort_STATUS_OK, "reply receive failed");
    return reply;
}

static void test_queue_mutex_and_task_flow(void)
{
    rtos_worker_context_t ctx = {0};
    eaPort_task_t worker_handle = NULL;
    eaPort_task_t lookup_handle = NULL;
    eaPort_task_info_t info = {0};
    int reply = 0;
    int expected_counter = 0;

    ctx.command_queue = eaPort_Queue_Create(TEST_QUEUE_DEPTH, sizeof(int));
    ctx.reply_queue = eaPort_Queue_Create(TEST_QUEUE_DEPTH, sizeof(int));
    ctx.mutex = eaPort_Mutex_Create();

    expect_true("command queue create", ctx.command_queue != NULL, "command queue creation failed");
    expect_true("reply queue create", ctx.reply_queue != NULL, "reply queue creation failed");
    expect_true("mutex create", ctx.mutex != NULL, "mutex creation failed");
    expect_true("initial command queue empty", eaPort_Queue_Messages_Waiting(ctx.command_queue) == 0U, "command queue should start empty");
    expect_true("initial reply queue empty", eaPort_Queue_Messages_Waiting(ctx.reply_queue) == 0U, "reply queue should start empty");

    eaPort_Mutex_Enter(ctx.mutex);
    ctx.shared_counter = 10;
    eaPort_Mutex_Exit(ctx.mutex);
    expected_counter = 10;

    expect_true("worker create", eaPort_Task_Create_Pinned_to_Core(
                     rtos_worker_task,
                     TEST_WORKER_NAME,
                     TEST_STACK_WORDS,
                     &ctx,
                     TEST_PRIORITY,
                     &worker_handle,
                     eaPort_NO_AFFINITY) == eaPort_STATUS_OK,
                "worker task creation failed");
    expect_true("worker handle returned", worker_handle != NULL, "worker handle should be populated");
    expect_true("worker started", wait_until_true(&ctx.started, TEST_WAIT_MS), "worker did not start in time");
    expect_true("worker current handle captured", ctx.task_handle != NULL, "worker should capture its own handle");

    lookup_handle = eaPort_Get_Task_Handle_By_Name(TEST_WORKER_NAME);
    expect_true("worker lookup by name", lookup_handle == ctx.task_handle, "task lookup should match worker handle");

    {
        eaPort_task_t info_handle = lookup_handle;
        expect_true("worker info readable", eaPort_Get_Task_Info(&info, &info_handle) == eaPort_STATUS_OK, "worker info should be readable");
        expect_true("worker info handle", info.xTaskHandle == ctx.task_handle, "worker info handle mismatch");
        expect_true("worker info name", strcmp(info.pcTaskName, TEST_WORKER_NAME) == 0, "worker info name mismatch");
        expect_true("worker info priority", eaPort_Get_Task_Priority(&info) == TEST_PRIORITY, "worker priority mismatch");
        expect_true("worker blocked state", info.eCurrentState == eaPort_TaskState_Blocked || info.eCurrentState == eaPort_TaskState_Unknown,
                    "worker should be blocked on its queue or reported as unknown");
        expect_true("worker blocked state char", strchr("BU", eaPort_Get_Task_State_char(&info)) != NULL,
                    "worker state char should be blocked or unknown");
    }

    eaPort_Task_Suspend(worker_handle);
    eaPort_Delay_Milliseconds(TEST_DELAY_MS);
    {
        eaPort_task_t info_handle = worker_handle;
        expect_true("worker suspended info", eaPort_Get_Task_Info(&info, &info_handle) == eaPort_STATUS_OK, "suspended worker info should be readable");
        expect_true("worker suspended state", info.eCurrentState == eaPort_TaskState_Suspended || info.eCurrentState == eaPort_TaskState_Unknown,
                    "worker should report suspended state or unknown");
        expect_true("worker suspended state char", strchr("SU", eaPort_Get_Task_State_char(&info)) != NULL,
                    "worker suspended state char mismatch");
    }

    eaPort_Task_Resume(worker_handle);
    eaPort_Delay_Milliseconds(TEST_DELAY_MS);
    
    send_command(ctx.command_queue, TEST_CMD_INC, "worker inc command 1");
    reply = receive_reply(ctx.reply_queue, "worker reply 1");
    expected_counter += 1;
    expect_true("worker increment reply 1", reply == expected_counter, "unexpected counter reply after first increment");
    expect_true("reply queue drained 1", eaPort_Queue_Messages_Waiting(ctx.reply_queue) == 0U, "reply queue should be empty after receive");

    send_command(ctx.command_queue, TEST_CMD_INC, "worker inc command 2");
    reply = receive_reply(ctx.reply_queue, "worker reply 2");
    expected_counter += 1;
    expect_true("worker increment reply 2", reply == expected_counter, "unexpected counter reply after second increment");

    send_command(ctx.command_queue, TEST_CMD_STOP, "worker stop command");
    expect_true("worker stop observed", wait_until_true(&ctx.stop_seen, TEST_WAIT_MS), "worker did not observe stop command");

    eaPort_Delay_Milliseconds(TEST_DELAY_MS);
    eaPort_Task_Delete(worker_handle);
    eaPort_Delay_Milliseconds(TEST_DELAY_MS);

    expect_true("worker lookup cleared", eaPort_Get_Task_Handle_By_Name(TEST_WORKER_NAME) == NULL, "task handle should be cleared after delete");
    expect_true("worker command queue empty", eaPort_Queue_Messages_Waiting(ctx.command_queue) == 0U, "command queue should be empty after cleanup");

    eaPort_Queue_Delete(ctx.command_queue);
    eaPort_Queue_Delete(ctx.reply_queue);
    eaPort_Mutex_Destroy(ctx.mutex);

    pass("port RTOS queue, mutex, and task flow");
}

void test_port_rtos_run(void)
{
    printf("=== Port-RTOS test harness ===\n");
    eaPort_Delay_Milliseconds(10000U);
    test_helpers_reset_counts();

    test_null_safe_guards();
    test_task_info_null_guards();
    test_time_and_memory_helpers();
    test_state_translation_helpers();
    test_queue_mutex_and_task_flow();

    printf("=== Port-RTOS tests done: passes=%" PRIu32 " fails=%" PRIu32 " ===\n",
           test_helpers_pass_count(),
           test_helpers_fail_count());

    while (1) {
        eaPort_Delay_Milliseconds(5000U);
        printf("[HEARTBEAT] Port-RTOS harness alive. passes=%" PRIu32 " fails=%" PRIu32 "\n",
               test_helpers_pass_count(),
               test_helpers_fail_count());
    }
}
