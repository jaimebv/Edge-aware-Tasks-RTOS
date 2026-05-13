# Port Layer

This document explains the EEAA port layer: the portable types, the RTOS
abstraction, the board abstraction, and how to implement a new port.

The port layer is what keeps the task manager independent from the exact RTOS
and board implementation.

## 1. What the port layer does

The port layer translates a small portable API into concrete platform behavior.
It provides:

- task handles
- mutex handles
- queue handles
- delays
- task creation and deletion
- cycle counters and timing helpers
- core identification
- task-state inspection helpers

The rest of the system calls the port layer, not FreeRTOS directly.
That separation is one of the main reasons the framework stays maintainable.

## 2. The main port modules

### 2.1 `port_interface_types.h`

This header defines the **portable types** used across the framework.

#### Key types

- **`eaPort_task_t`**
  - opaque task handle
  - represented as a `void *`
  - actual meaning depends on the RTOS backend

- **`eaPort_mutex_t`**
  - opaque synchronization handle
  - represented as a `void *`

- **`eaPort_task_function_t`**
  - function pointer type for task entry points
  - all ports must use this signature

- **`eaPort_task_state_t`**
  - portable task-state enum
  - maps RTOS-specific states into a common set

- **`eaPort_task_info_t`**
  - portable task information record
  - used for task inspection and reporting

- **`eaPort_queue_t`**
  - opaque queue handle
  - used for message passing between tasks

- **`eaPort_status_t`**
  - standardized success/failure return code

- **`eaPort_tick_t`**
  - portable tick counter type

#### Important constants

- `eaPort_STATUS_OK`
- `eaPort_STATUS_ERROR`
- `eaPort_MUTEX_INIT`
- `eaPort_WAIT_FOREVER`
- `eaPort_NO_WAIT`
- `eaPort_NO_AFFINITY`

These types are intentionally small and generic.
They define the vocabulary used by the framework without binding it to a single
RTOS or chip family.

### 2.2 `port_board_esp32.h`

This header exposes the **ESP32 board-specific hardware helpers**.

#### Responsibilities

- report the current core ID
- report the number of cores
- read the CPU cycle counter
- report CPU frequency
- convert cycles to milliseconds

#### Why this module matters

Task monitoring and latency analysis need access to hardware timing.
The task manager itself should not know how to read Xtensa registers.
That detail belongs here.

### 2.3 `port_rtos_freertos.h`

This header exposes the **FreeRTOS-backed abstraction layer**.

#### Responsibilities

- allocate and free heap memory
- create and destroy mutexes
- create and destroy queues
- send and receive queue messages
- create, delete, suspend, and resume tasks
- inspect task state and runtime statistics
- delay tasks
- look up current task handles and task handles by name

This is the layer that turns the portable EEAA API into concrete FreeRTOS
behavior.

### 2.4 `platform_interface.h`

This header provides the **platform-neutral timing interface**.

It allows the project to reason about:

- core identity
- CPU cycles
- CPU frequency
- cycle-to-time conversion

without hard-coding the board implementation into higher-level code.

## 3. Data structures used by the port layer

### 3.1 Task handle and mutex handle types

The handles are intentionally opaque.
That means the task manager and higher layers should not depend on the concrete
FreeRTOS handle types.

This keeps the abstraction portable and easy to retarget.

### 3.2 `eaPort_task_info_t`

This structure is the portable task-status record.

Fields:

- `xTaskHandle` — handle of the task being described
- `pcTaskName` — human-readable task name
- `ulRunTimeCounter` — accumulated runtime counter
- `uxTaskNumber` — task number or ID
- `eCurrentState` — portable state enum
- `uxCurrentPriority` — current priority
- `uxStackHighWaterMark` — stack slack / watermark metric

The exact unit for stack watermark depends on the RTOS backend, so the port docs
must be read together with the implementation.

### 3.3 `eaPort_task_state_t`

Portable states:

- Running
- Ready
- Blocked
- Suspended
- Deleted
- Unknown

These provide a stable semantic layer even if the backend uses different names.

## 4. How the layers work together

### 4.1 From task manager to RTOS

The task manager asks the RTOS port to create queues and tasks.
The RTOS port translates those calls into FreeRTOS primitives.

### 4.2 From task manager to board layer

The task manager uses board helpers for:

- core ID
- tick timing
- CPU cycle counts
- timing conversion

### 4.3 From application to task manager

Applications and tests should interact with the task manager, not the port layer
directly, unless they are specifically testing the port itself.

## 5. Adding a new port

A new port should be implemented in three steps.

### Step 1: Define the portable type mapping

Start with `port_interface_types.h`.
A new backend must map the abstract handles and task state model to its own types.

### Step 2: Implement the RTOS layer

Provide the queue, mutex, task, delay, and inspection functions in a backend
file similar to `port_rtos_freertos.h/.c`.

The backend should:

- keep the function signatures compatible
- preserve the success/failure contract
- avoid exposing RTOS-specific details to higher layers

### Step 3: Implement the board layer

Provide the timing and hardware-specific helpers for the target board.
That includes:

- CPU cycles
- CPU frequency
- core ID
- cycle-to-ms conversion

### Step 4: Validate the backend with task-manager tests

Any new port must still satisfy the task-manager regression tests.
A port is not complete until the lifecycle behavior, queue flow, and cleanup
paths work under real execution.

## 6. Port design rules

### Keep the abstraction thin

The port layer should translate behavior, not invent policy.
Policy belongs higher up in the task manager.

### Preserve null-safe behavior

The public port functions should handle null inputs safely when the contract says
so.
This keeps teardown paths and failure cleanup reliable.

### Keep timing semantics explicit

Cycle counters and tick counters are not interchangeable.
The port documentation must clearly state which unit each helper returns.

### Keep ownership obvious

If a function creates a queue, the matching destroy function should own the
cleanup contract.
If a function returns a handle, the docs should say who owns it.

## 7. Submodule reference guide

### `port_interface_types`

Use this when you need the portable vocabulary.

### `port_rtos`

Use this when you need tasks, queues, mutexes, delays, or task inspection.

### `port_board`

Use this when you need hardware timing or core identity.

### `platform_interface`

Use this when you want the platform-neutral timing abstraction.

## 8. Relationship to the old port README

This page replaces the old include-tree port README style with a richer,
maintainable documentation page.
The canonical docs now live under `/docs/components/port/Port.md` and the Doxygen
reference in the public headers.

## 9. Doxygen reference

The exact declarations are documented in:

- `components/EEAA/include/port/port_interface_types.h`
- `components/EEAA/include/port/port_board_esp32.h`
- `components/EEAA/include/port/port_rtos_freertos.h`
- `components/EEAA/include/interfaces/platform_interface.h`
- generated Doxygen API pages
