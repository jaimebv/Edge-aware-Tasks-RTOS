/*
 * Task lifecycle test harness.
 *
 * This file is intentionally verbose and fully commented because it is meant
 * to exercise the task-manager APIs on real ESP32 hardware, not only compile
 * them.
 *
 * Coverage goals:
 * - validate null / invalid inputs
 * - create a single task and verify teardown
 * - create local and paired edge tasks
 * - soak the runtime registry with many live tasks
 * - repeat create/delete churn to expose reuse bugs
 * - exercise runtime accessor helpers
 * - force runtime registry expansion with > 4 live pair runtimes
 * - cover client-only destroy and full pair teardown
 * - cover destroy-by-name and destroy-by-task-index
 * - verify monitor snapshots and metric update helpers
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "core/task_manager.h"
#include "test_helpers.h"
#include "port/port_rtos.h"

void test_offloader_policy_suite(void);
void test_offloader_controller_suite(void);


#define TEST_PAIR_COUNT          6U
#define TEST_QUEUE_DEPTH         1U
#define TEST_MESSAGE_SIZE        sizeof(int)
#define TEST_PRIORITY            2U
#define TEST_CORE_ID             0U
#define TEST_CLIENT_STACK        2048U
#define TEST_SERVER_STACK        2048U
#define TEST_LOCAL_STACK         2048U
#define TEST_PERIOD_MS           1000U
#define TEST_CLIENT_WCET         200U
#define TEST_SERVER_WCET         800U
#define TEST_LOCAL_WCET          150U
#define TEST_MAE2EL_MS           1000U
#define TEST_DELAY_SENSITIVITY   50U
#define TEST_ENERGY_SENSITIVITY  50U

static const edge_task_pair_spec_t kPairSpec = {
    .queue_depth = TEST_QUEUE_DEPTH,
    .message_size = TEST_MESSAGE_SIZE,
};

static const char *const kPairBaseNames[TEST_PAIR_COUNT] = {
    "TL0",
    "TL1",
    "TL2",
    "TL3",
    "TL4",
    "TL5",
};

static const char *const kLocalBaseName = "TLOC";

static volatile bool g_pair_ready[TEST_PAIR_COUNT];
static volatile bool g_pair_roundtrip[TEST_PAIR_COUNT];
static volatile int g_pair_reply[TEST_PAIR_COUNT];
static volatile int g_pair_client_index[TEST_PAIR_COUNT];
static volatile int g_pair_server_index[TEST_PAIR_COUNT];
static volatile uint32_t g_pair_runtime_id[TEST_PAIR_COUNT];
static volatile bool g_local_ready;
static volatile int g_local_index;
static volatile uint32_t g_local_runtime_id;
static volatile edge_task_creation_failure_reason_t g_forced_creation_failure_reason = EDGE_TASK_CREATION_FAILURE_NONE;

bool edge_task_manager_test_hook_should_fail_creation(
    edge_task_creation_failure_reason_t reason,
    const char *task_name)
{
    (void)task_name;
    return reason == g_forced_creation_failure_reason;
}

static void set_creation_failure_reason(edge_task_creation_failure_reason_t reason)
{
    g_forced_creation_failure_reason = reason;
}

static void reset_runtime_indices(void)
{
    for (size_t i = 0; i < TEST_PAIR_COUNT; ++i) {
        g_pair_ready[i] = false;
        g_pair_roundtrip[i] = false;
        g_pair_reply[i] = 0;
        g_pair_client_index[i] = -1;
        g_pair_server_index[i] = -1;
        g_pair_runtime_id[i] = 0U;
    }
    g_local_ready = false;
    g_local_index = -1;
    g_local_runtime_id = 0U;
}

static bool wait_until(volatile bool *flag, uint32_t timeout_ms)
{
    const uint32_t step_ms = 10U;
    uint32_t waited = 0U;

    while (!*flag && waited < timeout_ms) {
        eaPort_Delay_Milliseconds(step_ms);
        waited += step_ms;
    }

    return *flag;
}

static void make_client_name(char *buf, size_t buf_size, size_t idx)
{
    snprintf(buf, buf_size, "%s-cl-0", kPairBaseNames[idx]);
}

static void make_server_name(char *buf, size_t buf_size, size_t idx)
{
    snprintf(buf, buf_size, "%s-sv-0", kPairBaseNames[idx]);
}

static int find_pair_slot_by_runtime(edge_task_pair_runtime_t *runtime)
{
    const char *client_name;
    char expected[48];

    if (runtime == NULL) {
        return -1;
    }

    client_name = edge_task_pair_client_name(runtime);
    if (client_name == NULL) {
        return -1;
    }

    for (size_t i = 0; i < TEST_PAIR_COUNT; ++i) {
        make_client_name(expected, sizeof(expected), i);
        if (strcmp(client_name, expected) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static int find_local_slot_by_runtime(edge_task_pair_runtime_t *runtime)
{
    const char *client_name;
    char expected[48];

    if (runtime == NULL) {
        return -1;
    }

    client_name = edge_task_pair_client_name(runtime);
    if (client_name == NULL) {
        return -1;
    }

    snprintf(expected, sizeof(expected), "%s-lc-0", kLocalBaseName);
    return (strcmp(client_name, expected) == 0) ? 0 : -1;
}

static void assert_runtime_accessors(edge_task_pair_runtime_t *runtime, bool local_mode)
{
    /*
     * Check all runtime accessors once from within the task context.
     * The runtime is shared, so the values should be stable for the pair.
     */
    if (runtime == NULL) {
        fail("runtime accessor coverage", "runtime is NULL");
        return;
    }

    const char *client_name = edge_task_pair_client_name(runtime);
    const char *server_name = edge_task_pair_server_name(runtime);
    const char *host_name = edge_task_pair_host_name(runtime);
    eaPort_queue_t c2s = edge_task_pair_queue_client_to_server(runtime);
    eaPort_queue_t s2c = edge_task_pair_queue_server_to_client(runtime);
    eaPort_task_t client_handle = edge_task_pair_client_handle(runtime);
    eaPort_task_t server_handle = edge_task_pair_server_handle(runtime);
    edge_task_pair_role_t role = edge_task_pair_role(runtime);
    uint32_t pair_id = edge_task_pair_id(runtime);
    int task_index = edge_task_pair_task_index(runtime);
    int peer_index = edge_task_pair_peer_index(runtime);

    /* Task handles may appear a moment after the task starts; retry briefly. */
    for (uint32_t spin = 0U; spin < 50U && client_handle == NULL; ++spin) {
        eaPort_Delay_Milliseconds(20U);
        client_handle = edge_task_pair_client_handle(runtime);
    }
    if (!local_mode) {
        for (uint32_t spin = 0U; spin < 50U && server_handle == NULL; ++spin) {
            eaPort_Delay_Milliseconds(20U);
            server_handle = edge_task_pair_server_handle(runtime);
        }
    }

    if (local_mode) {
        expect_true("local runtime role", role == EDGE_TASK_PAIR_LOCAL, "wrong local role");
    } else {
        expect_true("pair runtime role", role == EDGE_TASK_PAIR_CLIENT, "wrong pair role");
    }

    expect_true("runtime client name", client_name != NULL && client_name[0] != '\0', "client name missing");
    expect_true("runtime host name", host_name != NULL, "host name missing");
    expect_true("runtime queues", c2s != NULL && s2c != NULL, "runtime queues missing");
    expect_true("runtime client handle", client_handle != NULL, "client handle missing");
    expect_true("runtime server handle", local_mode ? true : (server_handle != NULL), "server handle missing");
    expect_true("runtime pair id", pair_id != 0U, "pair id should be non-zero");
    expect_true("runtime task index", task_index >= 0, "task index invalid");
    expect_true("runtime peer index", local_mode ? (peer_index < 0) : (peer_index >= 0), "peer index invalid");

    if (!local_mode) {
        expect_true("runtime server name", server_name != NULL && server_name[0] != '\0', "server name missing");
    }
}

static void pair_client_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int slot;
    int own_index;
    int peer_index;
    int tx = 1000;
    int rx = 0;

    if (runtime == NULL) {
        fail("pair client startup", "bad runtime context");
        eaPort_Task_Delete(NULL);
        return;
    }

    slot = find_pair_slot_by_runtime(runtime);
    own_index = -1;
    peer_index = -1;

    /* Give the creator time to store task handles and runtime indices. */
    eaPort_Delay_Milliseconds(100U);

    for (uint32_t spin = 0U; spin < 50U && (own_index < 0 || peer_index < 0); ++spin) {
        own_index = edge_task_pair_task_index(runtime);
        peer_index = edge_task_pair_peer_index(runtime);
        if (own_index >= 0 && peer_index >= 0) {
            break;
        }
        eaPort_Delay_Milliseconds(20U);
    }

    if (slot >= 0) {
        g_pair_ready[slot] = true;
        g_pair_client_index[slot] = own_index;
        g_pair_server_index[slot] = peer_index;
        g_pair_runtime_id[slot] = edge_task_pair_id(runtime);
    }
    assert_runtime_accessors(runtime, false);

    expect_true("client own index", own_index >= 0, "client index not found");
    expect_true("client peer index", peer_index >= 0, "server index not found");
    expect_true("client index accessor", edge_task_pair_task_index(runtime) == own_index, "task index mismatch");
    expect_true("client peer accessor", edge_task_pair_peer_index(runtime) == peer_index, "peer index mismatch");

    /* Send one message and wait for the reply so the queues are proven live. */
    if (eaPort_Queue_Send(edge_task_pair_queue_client_to_server(runtime), &tx, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
        fail("client queue send", "send failed");
        eaPort_Task_Delete(NULL);
        return;
    }

    if (eaPort_Queue_Receive(edge_task_pair_queue_server_to_client(runtime), &rx, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
        fail("client queue receive", "receive failed");
        eaPort_Task_Delete(NULL);
        return;
    }

    g_pair_reply[slot] = rx;
    g_pair_roundtrip[slot] = true;

    expect_true("client roundtrip value", rx == tx + 1, "unexpected reply value");
    pass("pair client runtime accessors");
    pass("pair client message roundtrip");

    /* Idle forever so the main test can destroy the task pair later. */
    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void pair_server_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int slot;
    int rx = 0;
    int tx = 0;
    int own_index;
    int peer_index;

    if (runtime == NULL) {
        fail("pair server startup", "bad runtime context");
        eaPort_Task_Delete(NULL);
        return;
    }

    slot = find_pair_slot_by_runtime(runtime);
    own_index = -1;
    peer_index = -1;

    eaPort_Delay_Milliseconds(100U);
    for (uint32_t spin = 0U; spin < 50U && own_index < 0; ++spin) {
        own_index = edge_task_pair_task_index(runtime);
        peer_index = edge_task_pair_peer_index(runtime);
        if (own_index >= 0) {
            break;
        }
        eaPort_Delay_Milliseconds(20U);
    }
    if (slot >= 0) {
        g_pair_client_index[slot] = own_index;
        g_pair_server_index[slot] = peer_index;
    }
    assert_runtime_accessors(runtime, false);

    /* Proof that the server task can see its own monitor entry. */
    expect_true("server own index", own_index >= 0, "server index not found");
    pass("pair server runtime accessors");

    if (eaPort_Queue_Receive(edge_task_pair_queue_client_to_server(runtime), &rx, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
        fail("server queue receive", "receive failed");
        eaPort_Task_Delete(NULL);
        return;
    }

    tx = rx + 1;
    if (eaPort_Queue_Send(edge_task_pair_queue_server_to_client(runtime), &tx, eaPort_WAIT_FOREVER) != eaPort_STATUS_OK) {
        fail("server queue send", "send failed");
        eaPort_Task_Delete(NULL);
        return;
    }

    /* Keep the server alive for the later cleanup tests. */
    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static void local_task(void *pvParameters)
{
    edge_task_pair_runtime_t *runtime = (edge_task_pair_runtime_t *)pvParameters;
    int local_index;

    if (runtime == NULL) {
        fail("local task startup", "bad runtime context");
        eaPort_Task_Delete(NULL);
        return;
    }

    local_index = find_local_slot_by_runtime(runtime);

    for (uint32_t spin = 0U; spin < 50U && local_index < 0; ++spin) {
        local_index = edge_task_pair_task_index(runtime);
        if (local_index >= 0) {
            break;
        }
        eaPort_Delay_Milliseconds(20U);
    }

    if (local_index < 0) {
        fail("local task startup", "bad runtime context");
        eaPort_Task_Delete(NULL);
        return;
    }

    eaPort_Delay_Milliseconds(100U);
    g_local_ready = true;
    g_local_index = local_index;
    g_local_runtime_id = edge_task_pair_id(runtime);
    assert_runtime_accessors(runtime, true);

    expect_true("local task index", local_index >= 0, "local index not found");
    pass("local runtime accessors");

    while (1) {
        eaPort_Delay_Milliseconds(1000U);
    }
}

static bool create_pair_task(const char *base_name, edge_task_type_t app_type, edge_task_execution_site_t site)
{
    edge_task_creation_result_t result = CreateEATaskPinnedToCoreEx(
               base_name,
               TEST_PRIORITY,
               pair_client_task,
               pair_server_task,
               TEST_CLIENT_STACK,
               TEST_SERVER_STACK,
               TEST_CORE_ID,
               app_type,
               TEST_MAE2EL_MS,
               TEST_DELAY_SENSITIVITY,
               TEST_ENERGY_SENSITIVITY,
               site,
               &kPairSpec,
               "127.0.0.1",
               TEST_PERIOD_MS,
               TEST_CLIENT_WCET,
               TEST_SERVER_WCET);

    return result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE;
}

static bool create_local_task(const char *base_name)
{
    edge_task_creation_result_t result = CreateEATaskPinnedToCoreEx(
               base_name,
               TEST_PRIORITY,
               local_task,
               local_task,
               TEST_LOCAL_STACK,
               TEST_LOCAL_STACK,
               TEST_CORE_ID,
               LOCAL,
               TEST_MAE2EL_MS,
               TEST_DELAY_SENSITIVITY,
               TEST_ENERGY_SENSITIVITY,
               LOCAL_EXECUTION,
               &kPairSpec,
               "0.0.0.0",
               TEST_PERIOD_MS,
               TEST_LOCAL_WCET,
               TEST_LOCAL_WCET);

    return result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE;
}

static void reset_pair_slot_observations(size_t slot)
{
    if (slot >= TEST_PAIR_COUNT) {
        return;
    }

    g_pair_ready[slot] = false;
    g_pair_roundtrip[slot] = false;
    g_pair_reply[slot] = 0;
    g_pair_client_index[slot] = -1;
    g_pair_server_index[slot] = -1;
    g_pair_runtime_id[slot] = 0U;
}

static void test_failed_queue_allocation(void)
{
    const size_t baseline = get_num_monitored_tasks();
    const edge_task_pair_spec_t huge_spec = {
        .queue_depth = 4096U,
        .message_size = 4096U,
    };
    edge_task_creation_result_t result = CreateEATaskPinnedToCoreEx(
        "QFAIL",
        TEST_PRIORITY,
        pair_client_task,
        pair_server_task,
        TEST_CLIENT_STACK,
        TEST_SERVER_STACK,
        TEST_CORE_ID,
        ENRICHED,
        TEST_MAE2EL_MS,
        TEST_DELAY_SENSITIVITY,
        TEST_ENERGY_SENSITIVITY,
        LOCAL_EXECUTION,
        &huge_spec,
        "127.0.0.1",
        TEST_PERIOD_MS,
        TEST_CLIENT_WCET,
        TEST_SERVER_WCET);

    expect_true("queue allocation failure reason",
                result.failure_reason == EDGE_TASK_CREATION_FAILURE_QUEUE_CLIENT ||
                result.failure_reason == EDGE_TASK_CREATION_FAILURE_QUEUE_SERVER,
                edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("queue allocation failure index", result.task_index < 0, "queue allocation should not return an index");
    expect_true("queue allocation cleanup", get_num_monitored_tasks() == baseline, "queue allocation failure leaked monitor state");
}

static void test_single_task_creation(void)
{
    const size_t baseline = get_num_monitored_tasks();
    task_snapshot_t snapshot = {0};

    reset_runtime_indices();
    expect_true("single local create", create_local_task(kLocalBaseName), "single local task creation failed");
    expect_true("single local ready", wait_until(&g_local_ready, 5000U), "single local task did not start in time");
    expect_true("single local runtime id", g_local_runtime_id != 0U, "single local runtime id should be non-zero");
    expect_true("single local monitor count", get_num_monitored_tasks() == baseline + 1U, "single local task should add one monitor");
    expect_true("single local snapshot valid", get_task_snapshot_by_index(g_local_index, &snapshot) && snapshot.valid, "single local snapshot invalid");
    expect_true("single local snapshot name", strcmp(snapshot.name, "TLOC-lc-0") == 0, "single local snapshot name mismatch");
    expect_true("single local destroy", edge_task_pair_destroy_by_task_index(g_local_index, EDGE_TASK_CLEANUP_CLIENT_ONLY) == 1, "single local destroy failed");
    expect_true("single local removed", get_task_snapshot_by_index(g_local_index, &snapshot) == false, "single local task should be gone");
    expect_true("single local cleanup", get_num_monitored_tasks() == baseline, "single local cleanup should restore baseline");

    reset_runtime_indices();
}

static void test_strong_rollback_semantics(void)
{
    const size_t baseline = get_num_monitored_tasks();
    edge_task_creation_result_t result = {0};
    int created_index;

    set_creation_failure_reason(EDGE_TASK_CREATION_FAILURE_QUEUE_SERVER);
    result = CreateEATaskPinnedToCoreEx(
        "RBQ",
        TEST_PRIORITY,
        pair_client_task,
        pair_server_task,
        TEST_CLIENT_STACK,
        TEST_SERVER_STACK,
        TEST_CORE_ID,
        ENRICHED,
        TEST_MAE2EL_MS,
        TEST_DELAY_SENSITIVITY,
        TEST_ENERGY_SENSITIVITY,
        LOCAL_EXECUTION,
        &kPairSpec,
        "127.0.0.1",
        TEST_PERIOD_MS,
        TEST_CLIENT_WCET,
        TEST_SERVER_WCET);
    expect_true("rollback queue failure reason", result.failure_reason == EDGE_TASK_CREATION_FAILURE_QUEUE_SERVER, edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("rollback queue failure index", result.task_index < 0, "queue failure should not return an index");
    expect_true("rollback queue monitor cleanup", get_num_monitored_tasks() == baseline, "queue failure leaked monitor state");

    set_creation_failure_reason(EDGE_TASK_CREATION_FAILURE_NONE);
    result = CreateEATaskPinnedToCoreEx(
        "RBQ",
        TEST_PRIORITY,
        pair_client_task,
        pair_server_task,
        TEST_CLIENT_STACK,
        TEST_SERVER_STACK,
        TEST_CORE_ID,
        ENRICHED,
        TEST_MAE2EL_MS,
        TEST_DELAY_SENSITIVITY,
        TEST_ENERGY_SENSITIVITY,
        LOCAL_EXECUTION,
        &kPairSpec,
        "127.0.0.1",
        TEST_PERIOD_MS,
        TEST_CLIENT_WCET,
        TEST_SERVER_WCET);
    expect_true("rollback queue retry succeeds", result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0, "queue retry should succeed");
    expect_true("rollback queue retry monitor count", get_num_monitored_tasks() == baseline + 2U, "queue retry should add client/server monitors");
    created_index = result.task_index;
    expect_true("rollback queue retry destroy", edge_task_pair_destroy_by_task_index(created_index, EDGE_TASK_CLEANUP_PAIR) == 1, "queue retry cleanup failed");
    expect_true("rollback queue retry cleanup", get_num_monitored_tasks() == baseline, "queue retry cleanup should restore baseline");

    set_creation_failure_reason(EDGE_TASK_CREATION_FAILURE_SERVER_TASK);
    result = CreateEATaskPinnedToCoreEx(
        "RBS",
        TEST_PRIORITY,
        pair_client_task,
        pair_server_task,
        TEST_CLIENT_STACK,
        TEST_SERVER_STACK,
        TEST_CORE_ID,
        ENRICHED,
        TEST_MAE2EL_MS,
        TEST_DELAY_SENSITIVITY,
        TEST_ENERGY_SENSITIVITY,
        LOCAL_EXECUTION,
        &kPairSpec,
        "127.0.0.1",
        TEST_PERIOD_MS,
        TEST_CLIENT_WCET,
        TEST_SERVER_WCET);
    expect_true("rollback server failure reason", result.failure_reason == EDGE_TASK_CREATION_FAILURE_SERVER_TASK, edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("rollback server failure index", result.task_index < 0, "server failure should not return an index");
    expect_true("rollback server monitor cleanup", get_num_monitored_tasks() == baseline, "server failure leaked monitor state");

    set_creation_failure_reason(EDGE_TASK_CREATION_FAILURE_NONE);
    result = CreateEATaskPinnedToCoreEx(
        "RBS",
        TEST_PRIORITY,
        pair_client_task,
        pair_server_task,
        TEST_CLIENT_STACK,
        TEST_SERVER_STACK,
        TEST_CORE_ID,
        ENRICHED,
        TEST_MAE2EL_MS,
        TEST_DELAY_SENSITIVITY,
        TEST_ENERGY_SENSITIVITY,
        LOCAL_EXECUTION,
        &kPairSpec,
        "127.0.0.1",
        TEST_PERIOD_MS,
        TEST_CLIENT_WCET,
        TEST_SERVER_WCET);
    expect_true("rollback server retry succeeds", result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0, "server retry should succeed");
    expect_true("rollback server retry monitor count", get_num_monitored_tasks() == baseline + 2U, "server retry should add client/server monitors");
    created_index = result.task_index;
    expect_true("rollback server retry destroy", edge_task_pair_destroy_by_task_index(created_index, EDGE_TASK_CLEANUP_PAIR) == 1, "server retry cleanup failed");
    expect_true("rollback server retry cleanup", get_num_monitored_tasks() == baseline, "server retry cleanup should restore baseline");

    set_creation_failure_reason(EDGE_TASK_CREATION_FAILURE_LOCAL_TASK);
    result = CreateEATaskPinnedToCoreEx(
        "RBL",
        TEST_PRIORITY,
        local_task,
        local_task,
        TEST_LOCAL_STACK,
        TEST_LOCAL_STACK,
        TEST_CORE_ID,
        LOCAL,
        TEST_MAE2EL_MS,
        TEST_DELAY_SENSITIVITY,
        TEST_ENERGY_SENSITIVITY,
        LOCAL_EXECUTION,
        &kPairSpec,
        "0.0.0.0",
        TEST_PERIOD_MS,
        TEST_LOCAL_WCET,
        TEST_LOCAL_WCET);
    expect_true("rollback local failure reason", result.failure_reason == EDGE_TASK_CREATION_FAILURE_LOCAL_TASK, edge_task_creation_failure_reason_to_string(result.failure_reason));
    expect_true("rollback local failure index", result.task_index < 0, "local failure should not return an index");
    expect_true("rollback local monitor cleanup", get_num_monitored_tasks() == baseline, "local failure leaked monitor state");

    set_creation_failure_reason(EDGE_TASK_CREATION_FAILURE_NONE);
    result = CreateEATaskPinnedToCoreEx(
        "RBL",
        TEST_PRIORITY,
        local_task,
        local_task,
        TEST_LOCAL_STACK,
        TEST_LOCAL_STACK,
        TEST_CORE_ID,
        LOCAL,
        TEST_MAE2EL_MS,
        TEST_DELAY_SENSITIVITY,
        TEST_ENERGY_SENSITIVITY,
        LOCAL_EXECUTION,
        &kPairSpec,
        "0.0.0.0",
        TEST_PERIOD_MS,
        TEST_LOCAL_WCET,
        TEST_LOCAL_WCET);
    expect_true("rollback local retry succeeds", result.failure_reason == EDGE_TASK_CREATION_FAILURE_NONE && result.task_index >= 0, "local retry should succeed");
    expect_true("rollback local retry monitor count", get_num_monitored_tasks() == baseline + 1U, "local retry should add one monitor");
    created_index = result.task_index;
    expect_true("rollback local retry destroy", edge_task_pair_destroy_by_task_index(created_index, EDGE_TASK_CLEANUP_CLIENT_ONLY) == 1, "local retry cleanup failed");
    expect_true("rollback local retry cleanup", get_num_monitored_tasks() == baseline, "local retry cleanup should restore baseline");

    pass("strong rollback semantics");
}

static void test_invalid_and_null_paths(void)
{
    edge_task_pair_spec_t bad_spec = {
        .queue_depth = 0U,
        .message_size = 0U,
    };

    expect_true("destroy null runtime", edge_task_pair_destroy(NULL, EDGE_TASK_CLEANUP_PAIR) == 0, "destroy(NULL) should be harmless");
    expect_true("destroy by invalid index", edge_task_pair_destroy_by_task_index(-1, EDGE_TASK_CLEANUP_PAIR) == 1, "invalid index should fail cleanly");
    edge_task_pair_runtime_release(NULL);
    expect_true("runtime release null", true, "runtime_release(NULL) should be harmless");

    expect_true("queue client accessor null", edge_task_pair_queue_client_to_server(NULL) == NULL, "client queue on NULL should be NULL");
    expect_true("queue server accessor null", edge_task_pair_queue_server_to_client(NULL) == NULL, "server queue on NULL should be NULL");
    expect_true("client handle accessor null", edge_task_pair_client_handle(NULL) == NULL, "client handle on NULL should be NULL");
    expect_true("server handle accessor null", edge_task_pair_server_handle(NULL) == NULL, "server handle on NULL should be NULL");
    expect_true("client name accessor null", edge_task_pair_client_name(NULL) == NULL, "client name on NULL should be NULL");
    expect_true("server name accessor null", edge_task_pair_server_name(NULL) == NULL, "server name on NULL should be NULL");
    expect_true("host accessor null", edge_task_pair_host_name(NULL) == NULL, "host on NULL should be NULL");
    expect_true("role accessor null", edge_task_pair_role(NULL) == EDGE_TASK_PAIR_LOCAL, "role on NULL should default to local");
    expect_true("id accessor null", edge_task_pair_id(NULL) == 0U, "id on NULL should be zero");
    expect_true("task index accessor null", edge_task_pair_task_index(NULL) == -1, "task index on NULL should be -1");
    expect_true("peer index accessor null", edge_task_pair_peer_index(NULL) == -1, "peer index on NULL should be -1");

    {
        task_snapshot_t dummy_snapshot = {0};
        expect_true("snapshot null inputs", get_task_snapshot_by_index(-1, NULL) == false, "snapshot_by_index(-1,NULL) should fail");
        expect_true("snapshot null index", get_task_snapshot_by_index(-1, &dummy_snapshot) == false, "snapshot_by_index(-1,ptr) should fail");
    }
    expect_true("signal invalid index", get_task_signal(-1) == -1, "invalid signal should be -1");
    expect_true("cpu cycles invalid index", get_task_cpu_cycles(-1) == 0U, "invalid cpu cycles should be zero");
    expect_true("data size invalid index", get_task_data_size(-1) == 0U, "invalid data size should be zero");
    expect_true("OE2EL invalid index", get_task_OE2EL(-1) == 0U, "invalid OE2EL should be zero");
    expect_true("WCET invalid index", get_task_WCET(-1) == 0U, "invalid WCET should be zero");
    expect_true("exec site invalid index", strcmp(get_task_ex_site_by_index(-1), "UNKNOWN") == 0, "invalid exec site should be unknown");
    expect_true("hyperperiod smoke", tasks_compute_hyperperiod() == 0U, "hyperperiod stub should be zero");
    expect_true("is client task positive", is_client_task("abc-cl-0") == 1, "client suffix should match");
    expect_true("is client task negative", is_client_task("abc-sv-0") == 0, "server suffix should not match");

    expect_true("reject invalid queue spec", CreateEATaskPinnedToCore(
                    "BAD",
                    TEST_PRIORITY,
                    local_task,
                    local_task,
                    TEST_LOCAL_STACK,
                    TEST_LOCAL_STACK,
                    TEST_CORE_ID,
                    LOCAL,
                    TEST_MAE2EL_MS,
                    TEST_DELAY_SENSITIVITY,
                    TEST_ENERGY_SENSITIVITY,
                    LOCAL_EXECUTION,
                    &bad_spec,
                    "0.0.0.0",
                    TEST_PERIOD_MS,
                    TEST_LOCAL_WCET,
                    TEST_LOCAL_WCET) == -1,
                "invalid queue spec should be rejected");

    pass("null and invalid API paths");
}

static void test_snapshot_correctness_after_updates(void)
{
    task_snapshot_t snapshot = {0};
    char local_name[32];
    char client_name[32];
    char server_name[32];

    int local_idx = g_local_index;
    int client_idx = g_pair_client_index[0];
    int server_idx = g_pair_server_index[0];

    expect_true("local index present", local_idx >= 0, "local task missing");
    expect_true("client index present", client_idx >= 0, "client task missing");
    expect_true("server index present", server_idx >= 0, "server task missing");

    snprintf(local_name, sizeof(local_name), "%s-lc-0", kLocalBaseName);
    make_client_name(client_name, sizeof(client_name), 0U);
    make_server_name(server_name, sizeof(server_name), 0U);

    expect_true("local snapshot valid", get_task_snapshot_by_index(local_idx, &snapshot) && snapshot.valid, "local snapshot invalid");
    expect_true("local snapshot name from cold state", strcmp(snapshot.name, local_name) == 0, "local snapshot name mismatch");
    expect_true("client snapshot valid", get_task_snapshot_by_index(client_idx, &snapshot) && snapshot.valid, "client snapshot invalid");
    expect_true("client snapshot name from cold state", strcmp(snapshot.name, client_name) == 0, "client snapshot name mismatch");
    expect_true("server snapshot valid", get_task_snapshot_by_index(server_idx, &snapshot) && snapshot.valid, "server snapshot invalid");
    expect_true("server snapshot name from cold state", strcmp(snapshot.name, server_name) == 0, "server snapshot name mismatch");

    expect_true("local exec site", strcmp(get_task_ex_site_by_index(local_idx), "LOCAL_EXECUTION") == 0, "local exec site mismatch");
    expect_true("client exec site", strcmp(get_task_ex_site_by_index(client_idx), "LOCAL_EXECUTION") == 0, "client exec site mismatch");

    /* Keep the name-based wrappers around only as display/snapshot compatibility. */
    expect_true("local snapshot wrapper", get_task_snapshot(local_name, &snapshot) && strcmp(snapshot.name, local_name) == 0, "local snapshot wrapper mismatch");
    expect_true("client snapshot wrapper", get_task_snapshot(client_name, &snapshot) && strcmp(snapshot.name, client_name) == 0, "client snapshot wrapper mismatch");
    expect_true("server snapshot wrapper", get_task_snapshot(server_name, &snapshot) && strcmp(snapshot.name, server_name) == 0, "server snapshot wrapper mismatch");
    expect_true("local exec site wrapper", strcmp(get_task_ex_site(local_name), "LOCAL_EXECUTION") == 0, "local exec site wrapper mismatch");
    expect_true("client exec site wrapper", strcmp(get_task_ex_site(client_name), "LOCAL_EXECUTION") == 0, "client exec site wrapper mismatch");

    update_task_metrics_by_index(client_idx, 11U, 22U, 33U, 44U, 55U);
    expect_true("client cpu cycles updated", get_task_cpu_cycles(client_idx) == 22U, "client cpu cycles mismatch");
    expect_true("client data size updated", get_task_data_size(client_idx) == 55U, "client data size mismatch");
    expect_true("client OE2EL updated", get_task_OE2EL(client_idx) == 11U, "client OE2EL mismatch");

    update_task_metrics_by_index(client_idx, 66U, 77U, 88U, 99U, 111U);
    expect_true("client cpu cycles updated again", get_task_cpu_cycles(client_idx) == 77U, "client cpu cycles mismatch after second update");
    expect_true("client data size updated again", get_task_data_size(client_idx) == 111U, "client data size mismatch after second update");
    expect_true("client WCET readable", get_task_WCET(client_idx) == TEST_CLIENT_WCET, "client WCET mismatch");

    update_task_metrics_OE2EL_by_index(client_idx, 123U);
    expect_true("client OE2EL update helper", get_task_OE2EL(client_idx) == 123U, "client OE2EL helper mismatch");

    expect_true("client snapshot refresh valid", get_task_snapshot_by_index(client_idx, &snapshot) && snapshot.valid, "client snapshot refresh invalid");
    expect_true("client snapshot cpu cycles from hot state", snapshot.cpu_cycles == 77U, "client snapshot cpu cycles mismatch");
    expect_true("client snapshot OE2EL from hot state", snapshot.OE2EL == 123U, "client snapshot OE2EL mismatch");
    expect_true("client snapshot period from cold state", snapshot.period == TEST_PERIOD_MS, "client snapshot period mismatch");
    expect_true("client snapshot WCET from cold state", snapshot.WCET == TEST_CLIENT_WCET, "client snapshot WCET mismatch");
    expect_true("client snapshot name from cold state", strcmp(snapshot.name, client_name) == 0, "client snapshot name mismatch after refresh");

    pass("monitor snapshot and metric update helpers");
}

static void test_cleanup_paths(void)
{
    int client_idx;
    int server_idx;
    int local_idx;
    task_snapshot_t snapshot = {0};

    /* Pair 0: split teardown to exercise client-only and full-pair branches. */
    client_idx = g_pair_client_index[0];
    server_idx = g_pair_server_index[0];
    expect_true("pair0 client index present", client_idx >= 0, "pair0 client index missing");
    expect_true("pair0 server index present", server_idx >= 0, "pair0 server index missing");

    expect_true("pair0 client-only destroy", edge_task_pair_destroy_by_task_index(client_idx, EDGE_TASK_CLEANUP_CLIENT_ONLY) == 1, "client-only destroy failed");
    expect_true("pair0 client removed", get_task_snapshot_by_index(client_idx, &snapshot) == false, "client should be gone");
    expect_true("pair0 server remains", get_task_snapshot_by_index(server_idx, &snapshot) && snapshot.valid, "server should remain after client-only destroy");
    expect_true("pair0 full destroy by index", edge_task_pair_destroy_by_task_index(server_idx, EDGE_TASK_CLEANUP_PAIR) == 1, "full destroy by index failed");
    expect_true("pair0 fully removed", get_task_snapshot_by_index(server_idx, &snapshot) == false, "server should be gone after full destroy");
    expect_true("pair0 stale destroy no-op", edge_task_pair_destroy_by_task_index(client_idx, EDGE_TASK_CLEANUP_PAIR) == 1, "stale destroy should be harmless");

    /* Pair 1: full teardown by index. */
    client_idx = g_pair_client_index[1];
    server_idx = g_pair_server_index[1];
    expect_true("pair1 server index present", server_idx >= 0, "pair1 server index missing");
    expect_true("pair1 full destroy by index", edge_task_pair_destroy_by_task_index(server_idx, EDGE_TASK_CLEANUP_PAIR) == 1, "destroy by index failed");
    expect_true("pair1 client removed", get_task_snapshot_by_index(client_idx, &snapshot) == false, "pair1 client should be gone");
    expect_true("pair1 server removed", get_task_snapshot_by_index(server_idx, &snapshot) == false, "pair1 server should be gone");

    /* Pair 2: full teardown by task index. */
    client_idx = g_pair_client_index[2];
    server_idx = g_pair_server_index[2];
    expect_true("pair2 client index present", client_idx >= 0, "pair2 client index missing");
    expect_true("pair2 full destroy by client index", edge_task_pair_destroy_by_task_index(client_idx, EDGE_TASK_CLEANUP_PAIR) == 1, "destroy by client index failed");
    expect_true("pair2 client removed", get_task_snapshot_by_index(client_idx, &snapshot) == false, "pair2 client should be gone");
    expect_true("pair2 server removed", get_task_snapshot_by_index(server_idx, &snapshot) == false, "pair2 server should be gone");

    /* Pair 3: cleanup by client index. */
    client_idx = g_pair_client_index[3];
    server_idx = g_pair_server_index[3];
    expect_true("pair3 client index present", client_idx >= 0, "pair3 client index missing");
    expect_true("pair3 full destroy by client index", edge_task_pair_destroy_by_task_index(client_idx, EDGE_TASK_CLEANUP_PAIR) == 1, "pair3 cleanup failed");
    expect_true("pair3 client removed", get_task_snapshot_by_index(client_idx, &snapshot) == false, "pair3 client should be gone");
    expect_true("pair3 server removed", get_task_snapshot_by_index(server_idx, &snapshot) == false, "pair3 server should be gone");

    /* Pair 4: full teardown by server index after runtime expansion has already happened. */
    client_idx = g_pair_client_index[4];
    server_idx = g_pair_server_index[4];
    expect_true("pair4 server index present", server_idx >= 0, "pair4 server index missing");
    expect_true("pair4 full destroy by server index", edge_task_pair_destroy_by_task_index(server_idx, EDGE_TASK_CLEANUP_PAIR) == 1, "pair4 cleanup failed");
    expect_true("pair4 client removed", get_task_snapshot_by_index(client_idx, &snapshot) == false, "pair4 client should be gone");
    expect_true("pair4 server removed", get_task_snapshot_by_index(server_idx, &snapshot) == false, "pair4 server should be gone");

    /* Pair 5: extra soak slot cleanup by client index. */
    client_idx = g_pair_client_index[5];
    server_idx = g_pair_server_index[5];
    expect_true("pair5 client index present", client_idx >= 0, "pair5 client index missing");
    expect_true("pair5 full destroy by client index", edge_task_pair_destroy_by_task_index(client_idx, EDGE_TASK_CLEANUP_PAIR) == 1, "pair5 cleanup failed");
    expect_true("pair5 client removed", get_task_snapshot_by_index(client_idx, &snapshot) == false, "pair5 client should be gone");
    expect_true("pair5 server removed", get_task_snapshot_by_index(server_idx, &snapshot) == false, "pair5 server should be gone");

    local_idx = g_local_index;
    expect_true("local index present", local_idx >= 0, "local index missing");
    expect_true("local destroy by index", edge_task_pair_destroy_by_task_index(local_idx, EDGE_TASK_CLEANUP_CLIENT_ONLY) == 1, "local destroy failed");
    expect_true("local task removed", get_task_snapshot_by_index(local_idx, &snapshot) == false, "local task should be gone");

    pass("cleanup branch coverage");
}

static void test_repeated_create_delete_cycles(void)
{
    const size_t cycle_count = 6U;
    task_snapshot_t snapshot = {0};
    uint32_t first_pair_id = 0U;
    int first_client_index = -1;
    int first_server_index = -1;

    for (size_t cycle = 0; cycle < cycle_count; ++cycle) {
        reset_pair_slot_observations(0U);
        expect_true("cycle pair create", create_pair_task(kPairBaseNames[0], ENRICHED, LOCAL_EXECUTION), "cycle pair creation failed");
        expect_true("cycle pair ready", wait_until(&g_pair_ready[0], 5000U), "cycle pair did not start in time");
        expect_true("cycle pair roundtrip", wait_until(&g_pair_roundtrip[0], 5000U), "cycle pair roundtrip did not complete");
        expect_true("cycle pair reply", g_pair_reply[0] == 1001, "cycle pair reply mismatch");
        expect_true("cycle pair runtime id", g_pair_runtime_id[0] != 0U, "cycle pair runtime id should be non-zero");

        if (cycle == 0U) {
            first_pair_id = g_pair_runtime_id[0];
            first_client_index = g_pair_client_index[0];
            first_server_index = g_pair_server_index[0];
        } else if (cycle == 1U) {
            expect_true("runtime id changed after deletion", g_pair_runtime_id[0] != first_pair_id, "runtime id should change after recreate");
            expect_true("client index reused after deletion", g_pair_client_index[0] == first_client_index, "client index should be reused after delete");
            expect_true("server index reused after deletion", g_pair_server_index[0] == first_server_index, "server index should be reused after delete");
        }

        expect_true("cycle pair destroy", edge_task_pair_destroy_by_task_index(g_pair_client_index[0], EDGE_TASK_CLEANUP_PAIR) == 1, "cycle pair cleanup failed");
        expect_true("cycle client removed", get_task_snapshot_by_index(g_pair_client_index[0], &snapshot) == false, "cycle client should be gone");
        expect_true("cycle server removed", get_task_snapshot_by_index(g_pair_server_index[0], &snapshot) == false, "cycle server should be gone");
        expect_true("cycle monitor cleanup", get_num_monitored_tasks() == 0U, "cycle cleanup should restore zero monitors");
    }

    pass("repeated create/delete cycles and runtime reuse");
}

static void test_live_soak_batch(void)
{
    size_t i;
    char base_name[32];

    reset_runtime_indices();

    snprintf(base_name, sizeof(base_name), "%s", kLocalBaseName);
    expect_true("create local task", create_local_task(base_name), "local task creation failed");

    for (i = 0; i < TEST_PAIR_COUNT; ++i) {
        expect_true("create pair task", create_pair_task(kPairBaseNames[i], ENRICHED, LOCAL_EXECUTION), "pair task creation failed");
    }

    for (i = 0; i < TEST_PAIR_COUNT; ++i) {
        char ready_name[48];
        snprintf(ready_name, sizeof(ready_name), "pair-%u ready", (unsigned)i);
        expect_true(ready_name, wait_until((volatile bool *)&g_pair_ready[i], 5000U), "pair did not start in time");
        expect_true("pair runtime id", g_pair_runtime_id[i] != 0U, "pair runtime id should be non-zero");
    }

    expect_true("local ready", wait_until(&g_local_ready, 5000U), "local task did not start in time");
    expect_true("local runtime id", g_local_runtime_id != 0U, "local runtime id should be non-zero");

    for (i = 0; i < TEST_PAIR_COUNT; ++i) {
        char roundtrip_name[48];
        snprintf(roundtrip_name, sizeof(roundtrip_name), "pair-%u roundtrip", (unsigned)i);
        expect_true(roundtrip_name, wait_until((volatile bool *)&g_pair_roundtrip[i], 5000U), "pair roundtrip did not complete");
        expect_true(roundtrip_name, g_pair_reply[i] == 1001, "unexpected roundtrip payload");
    }

    expect_true("monitored task count", get_num_monitored_tasks() == (TEST_PAIR_COUNT * 2U) + 1U, "unexpected number of monitored tasks");
    pass("runtime registry soak batch created");
}

static void test_route_mutation_hooks(void)
{
    const int client_idx = g_pair_client_index[0];
    const int server_idx = g_pair_server_index[0];
    const edge_task_pair_runtime_t *client_runtime = NULL;
    const edge_task_pair_runtime_t *server_runtime = NULL;
    char original_host[CONFIG_EA_MAX_TASK_NAME_LEN];
    const char *original_site = NULL;
    const char *new_host = "edge-router";

    expect_true("route hook client index", client_idx >= 0, "route hook client index missing");
    expect_true("route hook server index", server_idx >= 0, "route hook server index missing");
    expect_true("route hook invalid host index", edge_task_pair_set_host_by_index(-1, new_host) == false, "invalid host index should fail");
    expect_true("route hook null host", edge_task_pair_set_host_by_index(client_idx, NULL) == false, "null host should fail");
    expect_true("route hook invalid exec index", edge_task_pair_set_exec_site_by_index(-1, REMOTE_EXECUTION) == false, "invalid exec site index should fail");

    client_runtime = edge_task_pair_runtime_by_task_index(client_idx);
    server_runtime = edge_task_pair_runtime_by_task_index(server_idx);
    expect_true("route hook client runtime", client_runtime != NULL, "client runtime missing");
    expect_true("route hook server runtime", server_runtime != NULL, "server runtime missing");
    expect_true("route hook shared runtime", client_runtime == server_runtime, "client/server runtime should be shared");

    original_site = get_task_ex_site_by_index(client_idx);
    expect_true("route hook original site", original_site != NULL, "original execution site missing");
    expect_true("route hook original host", edge_task_pair_host_name(client_runtime) != NULL, "original host missing");
    snprintf(original_host, sizeof(original_host), "%s", edge_task_pair_host_name(client_runtime));

    expect_true("route hook set host", edge_task_pair_set_host_by_index(client_idx, new_host), "host update failed");
    client_runtime = edge_task_pair_runtime_by_task_index(client_idx);
    expect_true("route hook host updated", strcmp(edge_task_pair_host_name(client_runtime), new_host) == 0, "runtime host did not update");

    expect_true("route hook set remote", edge_task_pair_set_exec_site_by_index(client_idx, REMOTE_EXECUTION), "remote exec site update failed");
    expect_true("route hook remote site", strcmp(get_task_ex_site_by_index(client_idx), "REMOTE_EXECUTION") == 0, "exec site did not switch to remote");

    expect_true("route hook restore local site", edge_task_pair_set_exec_site_by_index(client_idx, LOCAL_EXECUTION), "local exec site restore failed");
    expect_true("route hook local site", strcmp(get_task_ex_site_by_index(client_idx), "LOCAL_EXECUTION") == 0, "exec site did not restore to local");

    expect_true("route hook restore host", edge_task_pair_set_host_by_index(client_idx, original_host), "host restore failed");
    client_runtime = edge_task_pair_runtime_by_task_index(client_idx);
    expect_true("route hook host restored", strcmp(edge_task_pair_host_name(client_runtime), original_host) == 0, "runtime host did not restore");
    expect_true("route hook original site stable", strcmp(get_task_ex_site_by_index(client_idx), original_site) == 0, "original site should stay stable");

    pass("route mutation hooks");
}

void app_main(void)
{
    printf("=== Task lifecycle test harness ===\n");
    task_manager_init();
    test_helpers_reset_counts();
    reset_runtime_indices();
    set_creation_failure_reason(EDGE_TASK_CREATION_FAILURE_NONE);

    test_offloader_policy_suite();
    test_offloader_controller_suite();
    test_invalid_and_null_paths();
    test_strong_rollback_semantics();

    test_failed_queue_allocation();
    test_single_task_creation();
    test_live_soak_batch();
    test_route_mutation_hooks();
    test_snapshot_correctness_after_updates();
    test_cleanup_paths();

    expect_true("all monitors cleared", get_num_monitored_tasks() == 0U, "monitor count should be zero after cleanup");
    test_repeated_create_delete_cycles();
    expect_true("all monitors cleared after churn", get_num_monitored_tasks() == 0U, "monitor count should return to zero after churn");
    pass("runtime registry expansion and cleanup complete");

    printf("=== Task lifecycle tests done: passes=%" PRIu32 " fails=%" PRIu32 " ===\n",
           test_helpers_pass_count(),
           test_helpers_fail_count());

}
