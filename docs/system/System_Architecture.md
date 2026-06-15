# System Architecture

## 1. Purpose of the system

Edge-aware Tasks RTOS is an ESP32 / ESP-IDF framework for building and observing
**edge tasks**: task pairs and single local tasks that carry explicit execution
metadata, message queues, and monitoring state.

The system is not a generic application shell. It exists to answer a specific
embedded-systems problem:

- how to create tasks that can represent work split between a local client and a
  cooperating server role
- how to keep that task relationship observable at runtime
- how to make creation, cleanup, reuse, and failure handling predictable on a
  constrained device
- how to preserve a portable API boundary so the runtime can be adapted to
  other ports later

The result is a framework that behaves like a small runtime for edge-oriented
workloads. It exposes enough structure to reason about scheduling and data flow,
while keeping the implementation focused on embedded constraints.

## 2. Design objectives

The architecture is built around these goals:

1. **Portability**
   - keep the application-facing task manager independent from the concrete
     RTOS or board implementation
   - isolate FreeRTOS and ESP32 specifics inside the port layer

2. **Observability**
   - every created task should have a monitored identity
   - task snapshots should reflect both hot runtime state and cold descriptive
     metadata
   - runtime accessors should make it possible to inspect a task pair from inside
     the task context
   - runtime diagnostics should expose controller route/failure events to
     product users and board tests

3. **Cleanup correctness**
   - creation must roll back cleanly when a queue or task step fails
   - deletion must release queues, task handles, monitor entries, and runtime
     slots without leaking stale state
   - repeated create/delete cycles must reuse resources safely

4. **Predictable resource use**
   - heap allocations must be explicit
   - runtime registry growth must be bounded and visible
   - task and queue ownership must remain clear

5. **Contributor clarity**
   - the module boundaries must be obvious
   - tests and documentation should explain how the system behaves, not only how
     to call it

## 3. High-level architecture

The system is organized in layers:

```text
Application / tests / examples
        |
        v
Offloader controller (client routing)
        |
        v
EEAA task manager (core/task_manager)
        |
        +--> Monitoring store (hot + cold state)
        +--> Runtime registry (pair ownership, queues, handles)
        +--> Creation / teardown orchestration
        |
        v
Portable RTOS layer (port/port_rtos)
        |
        v
Board layer (port/port_board)
        |
        v
ESP32 + FreeRTOS + hardware
```

### Layer responsibilities

- **Application / tests / examples**
  - define task behavior
  - create task pairs or local tasks
  - inspect snapshots and metrics
  - validate cleanup and reuse

- **Offloader controller**
  - reads task-manager snapshots for client-side EA tasks
  - evaluates a routing policy for the client payload
  - applies LOCAL or REMOTE routing through task-manager helpers only
  - keeps route mutation separate from the task manager implementation

- **Runtime facade**
  - owns the developer-facing runtime lifecycle
  - configures and starts the task manager plus offloader together
  - exposes runtime status and controller ticks through a stable public API
  - keeps application code away from task-manager and offloader ownership

- **EEAA task manager**
  - owns task creation policy
  - owns runtime object lifetimes
  - stores monitoring state
  - resolves name/index/runtime relationships

- **Portable RTOS layer**
  - creates/destroys tasks, queues, mutexes, and delays through a stable
    abstract interface

- **Board layer**
  - exposes CPU frequency, cycle counters, and core identity
  - isolates ESP32-specific hardware details

- **Hardware / FreeRTOS**
  - runs the actual scheduler and provides the runtime environment

## 4. System behavior from creation to teardown

### 4.1 Task creation

Task creation is the central flow of the framework.

A caller provides:

- a task name
- client and server task entry functions
- stack sizes
- priority and core affinity
- execution type and execution site
- queue specification
- host metadata
- timing / monitoring parameters

The task manager then:

1. validates the inputs
2. reserves a runtime slot
3. creates the queues
4. allocates a pair identifier
5. creates the client task
6. creates the server task when the execution site requires it
7. registers monitor entries for every live task
8. exposes the runtime through accessors and task indices

If any step fails, the manager rolls back the partial work.
This is essential because embedded systems cannot afford inconsistent state.

### 4.2 Runtime observation

Once tasks exist, there are two different kinds of observation:

- **hot monitoring**: low-latency runtime state used on the execution path
- **cold monitoring**: descriptive metadata used for lookup, reporting, and
  debugging

The task manager keeps the two in sync through the same task index and pair ID.
That means the application can query either live counters or static identity
without duplicating ownership logic.

### 4.3 Task execution and message flow

For paired tasks, the message flow is intentionally explicit:

- the client writes into the client-to-server queue
- the server reads from that queue
- the server processes the payload
- the server writes a response into the server-to-client queue
- the client reads the response

That queue pair is the communication boundary between the roles.
It avoids hidden shared memory protocols and makes the data flow visible in the
code and in the tests.

### 4.4 Teardown and reuse

Teardown is handled in a way that supports both partial and full cleanup:

- **client-only cleanup** removes the client/local task and its monitor entry
- **pair cleanup** removes both tasks, both queues, all monitor entries, and the
  runtime slot itself

This is important for regression testing because it exposes:

- stale monitor entries
- leaked runtime slots
- broken queue ownership
- reuse bugs after deletion
- fragmentation in the runtime registry

## 5. Module responsibilities

### 5.1 `core/task_manager`

This is the core of the system.

It owns:

- task pair creation APIs
- snapshot APIs
- metric update helpers
- runtime accessors
- destroy / cleanup helpers
- the monitoring store
- the runtime registry used by pair allocations

It is the module that most directly defines the user-visible semantics of the
framework.

### 5.2 `api/runtime`

This is the product-grade facade for v1 applications.

It owns:

- runtime configuration defaults
- runtime start/stop control
- a thin controller tick wrapper
- runtime status snapshots for application code and tests
- runtime diagnostics snapshots for route/failure observability

It does not own task creation or routing logic itself.
Instead, it coordinates the task manager and offloader so application code can
use one stable entry point.

### 5.3 `port/port_interface_types`

This defines the portable types used by the rest of the system.

It exists so the code can speak in RTOS-neutral terms:

- task handles
- mutex handles
- queue handles
- tick types
- task information records
- task state enums
- return codes

### 5.4 `port/port_rtos_freertos`

This maps the portable abstractions to FreeRTOS.

It is the task/queue/mutex runtime backend.

### 5.4 `port/port_board_esp32`

This exposes the hardware-specific timing and core-identification features of
ESP32.

### 5.5 `interfaces/platform_interface`

This defines the platform-neutral view of timing and CPU-cycle conversion.

It is the bridge used when the system wants to reason about time without caring
which board implementation is active.

### 5.6 `examples`

The examples demonstrate the intended usage patterns.

They are not production application logic; they are reference flows that show how
tasks, queues, and monitoring work together.

### 5.7 `test`

The lifecycle tests are the regression harness.

They are meant to prove that creation, cleanup, failure rollback, snapshot
consistency, and runtime reuse remain correct under stress.

## 6. Information flow between modules

### 6.1 Creation flow

```text
caller -> task_manager -> port_rtos -> FreeRTOS
                    -> monitoring store
                    -> runtime registry
```

The caller never talks directly to FreeRTOS. The task manager coordinates the
flow and populates the monitoring data.

### 6.2 Observation flow

```text
caller -> task_manager accessors -> monitoring store / runtime registry
```

Snapshots and accessors read from the monitored state, not from ad hoc app-level
variables.

### 6.3 Message passing flow

```text
client task -> queue(client->server) -> server task -> queue(server->client) -> client task
```

That flow is the foundation for the pair model.

### 6.4 Teardown flow

```text
caller -> task_manager destroy API -> runtime registry cleanup
                                   -> queue deletion
                                   -> task deletion
                                   -> monitoring cleanup
                                   -> runtime release
```

Cleanup is intentionally multi-step because each resource has distinct ownership.

### 6.5 Offloader routing flow

```text
task_manager snapshots -> offloader candidate scan -> policy evaluation
                        -> task_manager route mutation
```

Batch/vector mode adds an explicit planning stage:

```text
task_manager snapshots -> offloader candidate scan -> batch policy planning
                        -> offloading vector -> validation
                        -> task_manager route mutation
```

The offloader only considers client segments when it scans for candidates.
The controller then writes the resulting route back through the task manager so
the local server half or the remote host path can be selected consistently.

## 7. Why edge tasks matter

An edge task is more than a task name.
It is a unit of work that carries:

- ownership
- execution site
- period and WCET constraints
- host metadata
- queue contracts
- monitor entries
- runtime identity

That matters because edge systems frequently need to reason about work that may
move between local and remote execution contexts.
The framework gives that movement a concrete model instead of leaving it implicit.

## 8. Relationship to production quality

The system is designed to be production-ready in the embedded sense:

- failures are explicit
- resources are paired with cleanup paths
- tests verify the hard paths, not just the happy path
- documentation explains ownership and contracts
- the runtime API is small enough to inspect and reason about

## 9. Documentation references

- [Task Management](../components/tasks/Task_Management.md)
- [Port Layer](../components/port/Port.md)
- [System Configuration](SystemConfig.md)
- Doxygen API reference from public headers
