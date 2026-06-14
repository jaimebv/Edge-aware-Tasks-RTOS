# Task Management

This document explains the EEAA task manager in operational terms.
The goal is to make the lifecycle of an edge task understandable from the point
of view of a user, a contributor, or a maintainer who needs to extend the code.

The Doxygen API reference gives you the function-by-function declarations.
This page explains the design behind those declarations.

## 1. What the task manager is responsible for

The task manager is the central runtime service of the framework.
It owns:

- task creation and registration
- paired runtime state
- queue allocation for client/server communication
- hot and cold monitoring data
- task snapshots
- cleanup and teardown
- compatibility wrappers for legacy name-based helpers

It is not a generic task factory.
It exists to create **edge tasks** that carry identity, monitoring metadata, and
explicit communication structure.

## 2. The edge-task model

An edge task is represented by one of two shapes:

1. **paired task**
   - a client role and a server role share a runtime record
   - they communicate through two queues
   - they are tracked as a related pair

2. **local task**
   - a single task with no server half
   - still gets a runtime record and a monitor entry
   - still participates in the same monitoring and cleanup model

This model makes the system suitable for edge-oriented workloads where some work
is locally executed and some work is split between cooperating execution roles.

## 3. Creation flow

The creation API is centered on the public task spec `edge_task_spec_t`,
`CreateEATaskFromSpecEx()`, and the legacy wrapper `CreateEATaskPinnedToCore()`.

The public task spec is the developer-facing shape for v1:

- it freezes the fields that describe a task declaration
- it validates the structure before it reaches the task manager internals
- it maps cleanly onto the existing creation path

`CreateEATaskPinnedToCoreEx()` remains available as the lower-level creation
entry point for compatibility and internal use.

### 3.1 Inputs

A task creation call provides:

- `TaskName`
- task priority
- client and server task entry functions
- client and server stack sizes
- core affinity
- application type (`LOCAL`, `ENRICHED`, `REMOTE`)
- MAE2EL / delay sensitivity / energy sensitivity parameters
- execution site (`LOCAL_EXECUTION` or `REMOTE_EXECUTION`)
- queue specification
- host name
- period and WCET values

### 3.2 Validation

The task manager first checks that the call is structurally valid.
This includes:

- non-null task entry points
- valid queue spec
- valid parameter combinations for the requested task type

If validation fails, the result reports `EDGE_TASK_CREATION_FAILURE_INVALID_SPEC`.

### 3.3 Runtime reservation

The task manager reserves a runtime slot before creating the tasks.
That runtime slot is the shared ownership record for the pair or local task.

The runtime slot is important because it holds:

- queue handles
- task handles
- pair ID
- client/server indices
- role
- names
- host label
- lifecycle state

### 3.4 Queue creation

A paired task uses two queues:

- **client -> server**
- **server -> client**

The queues are created before task startup so that both roles can immediately use
them once their task function begins executing.

If queue creation fails, the runtime is rolled back and the failure reason is
reported precisely.

### 3.5 Task startup

The task manager creates the client task first.
If the execution site requires the server half, it then creates the server task.

Local tasks use a single task entry and a local naming form.

### 3.6 Monitor registration

Each task gets a monitor slot in the global monitoring store.
The monitor entry is linked to the runtime through the pair ID and the task index.

This link is what lets the system answer questions like:

- which task belongs to which pair?
- what is the current snapshot of this task?
- what queue pair belongs to this runtime?
- is this the client or the server half?

## 4. Runtime structure

The runtime is an opaque pointer publicly, but internally it stores the ownership
state shared by the task pair.

The public type is `edge_task_pair_runtime_t`, and the task manager treats it as
a borrowed view that is only valid while the runtime remains active.

Conceptually, the runtime contains:

- client queue handle
- server queue handle
- client task handle
- server task handle
- pair ID
- client index
- server index
- lifecycle state
- client name
- server name
- host label
- role

### Why the runtime exists

It solves three problems at once:

1. **ownership**
   - the caller does not own the queue handles or task handles directly

2. **coordination**
   - both tasks can refer to the same shared pair metadata

3. **teardown**
   - the destroy path can find and release everything consistently

## 5. Hot and cold monitoring

The task manager splits monitoring into **hot** and **cold** data.

The actual structures are:

- `edge_task_monitor_hot_t`
- `edge_task_monitor_cold_t`
- `edge_task_monitor_t` as the compatibility aggregate that contains both

The helper `edge_task_monitor_sample_t` captures the start tick and start cycle
count for a measurement interval.

### 5.1 Hot monitoring

Hot state is updated on the execution path.
It is kept small so it can be touched frequently without carrying heavy metadata.

The hot structure contains:

- `pair_id`
- `task_index`
- `peer_index`
- `cpu_cycles`
- `data_size`
- `start_tick`
- `end_tick`
- `OE2EL`
- `signal_request`
- `core`
- `is_active`

#### Meaning of the hot fields

- **pair_id**: stable identity shared by both halves of a pair
- **task_index**: monitored index of the current task
- **peer_index**: monitored index of the opposite role
- **cpu_cycles**: execution cycle accounting
- **data_size**: payload size associated with the last monitored exchange
- **start_tick / end_tick**: timing boundaries for the monitored interval
- **OE2EL**: observed end-to-end latency metric
- **signal_request**: control signal requested for the task
- **core**: CPU core the task is currently associated with
- **is_active**: whether the monitor slot is valid

### 5.2 Cold monitoring

Cold state is updated more rarely and stores the descriptive metadata.
It contains:

- task name
- host label
- pair ID
- task index
- peer index
- period
- MAE2EL
- WCET
- execution site
- delay weight
- energy weight

#### Meaning of the cold fields

- **name**: human-readable task identity
- **host**: origin or host label attached to the task
- **pair_id**: same identity as the hot state
- **task_index**: same index as the hot state
- **peer_index**: same peer linkage as the hot state
- **period**: scheduling period used for the task
- **MAE2EL**: configuration for max acceptable end-to-end latency
- **WCET**: worst-case execution-time budget for the monitored side
- **exec_site**: local or remote execution mode
- **delay_weight / energy_weight**: tuning weights used by the model

### 5.3 Route mutation helpers

The task manager also exposes narrow route-mutation helpers for the offloader
controller:

- `edge_task_pair_runtime_by_task_index()`
- `edge_task_pair_set_host_by_index()`
- `edge_task_pair_set_exec_site_by_index()`

These helpers let the controller update routing metadata without reaching into
the runtime registry directly.

### 5.4 Why the split matters

The split keeps the fast path small while preserving rich metadata.
That is a common embedded design pattern:

- keep the data that changes frequently close to the runtime path
- keep descriptive state elsewhere
- link both using an index and a stable pair ID

### 5.5 Relating hot and cold state

The hot and cold structures are matched by:

- task index
- pair ID
- the monitor slot position itself

The hot state answers "what is happening right now?"
The cold state answers "what is this task supposed to be?"

A snapshot combines both views into a single user-facing record.

## 6. Snapshots

`task_snapshot_t` is the stable user-facing view of a task.
It is the compact report structure used by the snapshot helpers.
It contains:

- name
- cpu_cycles
- period
- WCET
- OE2EL
- valid flag

This structure is intentionally smaller than the full monitor store.
It is meant for reporting and inspection, not for ownership.

### Snapshot behavior

- snapshots are valid only while the task is active
- invalid or destroyed tasks return false
- snapshot refresh pulls the latest hot and cold values together

## 7. Message passing and queue role

Paired tasks communicate through two queues.
The queue pair makes the relationship between the roles explicit.

### Client-to-server queue

The client sends request messages through the client-to-server queue.
The server reads from this queue.

### Server-to-client queue

The server replies through the server-to-client queue.
The client reads from this queue.

### Why two queues instead of one

Two queues make the traffic direction obvious.
That separation helps with:

- debugging
- deterministic testing
- avoiding bidirectional message ambiguity
- measuring request/response behavior

## 8. Local task role

A local task is a task with no server half.
It still uses the same task-manager infrastructure, but with a simplified runtime
shape.

A local task is useful when:

- work does not need offloading
- a single execution unit is enough
- you still want monitoring and cleanup support

## 9. Cleanup semantics

The destroy API supports two modes:

- `EDGE_TASK_CLEANUP_CLIENT_ONLY`
- `EDGE_TASK_CLEANUP_PAIR`

### Client-only cleanup

Removes the client/local task and its monitor entry.
The server half may remain valid in paired cases.

### Pair cleanup

Removes both halves, both queues, the monitor entries, and the runtime slot.

### Why this matters

The distinction lets tests and callers exercise partial cleanup and full cleanup
separately, which is critical for catching stale references and lifecycle bugs.

## 10. Failure reporting

The structured creation API returns both an index and a failure reason.
The failure reasons include:

- invalid spec
- runtime slot exhaustion
- client queue failure
- server queue failure
- client task failure
- server task failure
- local task failure

That separation is important because "creation failed" is not enough information
when debugging embedded cleanup behavior.

## 11. Accessors and compatibility wrappers

The task manager exposes runtime accessors for:

- queue handles
- task handles
- task names
- host name
- pair ID
- task index
- peer index
- role

It also still provides some legacy name-based helpers.
These are compatibility wrappers and should be treated as secondary APIs.

The structured creation result is `edge_task_creation_result_t`, which returns
both a task index and a precise `edge_task_creation_failure_reason_t`.

New code should prefer:

- runtime accessors when a runtime pointer exists
- index-based helpers when the task index is known

## 12. Internal registry behavior

The runtime registry stores runtime slots in chunks.
The tests intentionally force growth beyond the initial chunk size to expose:

- expansion bugs
- reuse bugs
- stale runtime pointers
- cleanup holes

This is part of why the module is regression-tested heavily.

## 13. Test coverage

The lifecycle tests in `test/test_task_lifecycle.c` cover:

- invalid inputs
- failure rollback
- single-task creation
- soak creation of many live tasks
- repeated create/delete cycles
- queue failure handling
- snapshot correctness after updates
- runtime reuse after deletion
- cleanup branches

See [Task Manager Tests](../tests/task_manager.md) for the test-by-test explanation.

## 14. Module specification reference

The canonical API contract is documented in the Doxygen headers, especially:

- `components/EEAA/include/core/task_manager.h`
- generated Doxygen API pages

Use the Doxygen reference for exact signatures and this document for system-level understanding.
