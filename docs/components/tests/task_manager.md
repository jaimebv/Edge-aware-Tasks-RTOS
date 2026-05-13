# Task Manager Tests

This page explains the regression coverage implemented in
`test/test_task_lifecycle.c`.

The test file is intentionally long and verbose because it is doing real
embedded regression work:

- it creates live tasks on hardware
- it exercises the runtime registry under pressure
- it checks cleanup after success and failure
- it validates runtime reuse after deletion
- it verifies snapshot correctness after metric updates

The tests are not meant to be a unit-test toy example.
They are a hardware-backed lifecycle harness.

## 1. Test philosophy

The task manager is the most stateful part of the framework.
That means the dangerous bugs are not the obvious ones.
The dangerous bugs are:

- stale runtime slots
- queue leaks
- task leaks
- stale snapshot data
- name/index mismatches
- failed rollback paths
- reuse after deletion
- cleanup behavior that works once but not under churn

The tests are designed to catch exactly those problems.

## 2. Test harness structure

The test file defines:

- task entry points for paired client/server tasks
- a local task entry point
- helper functions for creating tasks
- helper functions for waiting on readiness flags
- helper functions for comparing task names and indices
- a weak test hook that can force creation failures

The tests run on real ESP32 hardware through PlatformIO.

## 3. Coverage map

### 3.1 `test_invalid_and_null_paths()`

This test verifies that the API behaves safely on invalid input.

It checks things like:

- destroy with null runtime
- destroy with invalid index
- runtime release with null
- null-safe runtime accessors
- invalid snapshot lookup
- invalid metric access
- invalid execution-site lookup
- bad queue spec rejection
- helper predicates like `is_client_task()`

#### Why it matters

Null-safe behavior is part of the cleanup contract.
A task-manager API that fails unpredictably on invalid inputs is hard to use
safely in embedded code.

### 3.2 `test_strong_rollback_semantics()`

This test forces the structured creation path through multiple failure modes:

- queue server failure
- server-task failure
- local-task failure

Then it repeats the same calls without forcing failure to prove that the rollback
path did not leave stale state behind.

#### Why it matters

This is the regression coverage for partial creation failures.
It proves that the task manager can recover after a failed attempt.

### 3.3 `test_failed_queue_allocation()`

This test attempts to create a task with an intentionally huge queue spec.

It is meant to expose queue allocation failure and verify that the monitor state
is not leaked when the queues cannot be created.

#### Why it matters

Queue allocation is one of the most likely fragmentation points.
This test catches failures where queue creation returns null but cleanup is not
completed properly.

### 3.4 `test_single_task_creation()`

This test creates one local task, waits for it to start, checks its snapshot,
and destroys it cleanly.

#### What it proves

- one task can be created successfully
- a local-only runtime works
- the snapshot is valid
- teardown restores the baseline monitor count

#### Why it matters

A framework should work for the simplest case before it claims to handle the
stress case.

### 3.5 `test_live_soak_batch()`

This test creates:

- one local task
- six live pair tasks

It waits for each task to become ready and for each pair to complete its
round-trip message exchange.

#### What it proves

- many live tasks can coexist
- the runtime registry expands beyond its first chunk
- queues remain functional under load
- monitor counts match the expected live task count

#### Why it matters

This is the soak-style test that exposes fragmentation and registry growth bugs.

### 3.6 `test_snapshot_correctness_after_updates()`

This test checks that snapshots reflect the right mix of hot and cold data after
metric updates.

It verifies:

- name resolution
- execution-site reporting
- metric updates by index
- snapshot refresh behavior
- cold-state values like period and WCET
- hot-state values like CPU cycles and OE2EL

#### Why it matters

Snapshots are only useful if they remain coherent after runtime updates.

### 3.7 `test_cleanup_paths()`

This test exercises several cleanup combinations:

- client-only teardown
- full pair teardown by client index
- full pair teardown by server index
- stale destroy no-op behavior
- local task teardown

#### Why it matters

Cleanup is where stale pointers and ownership bugs usually appear.
This test proves that the module handles mixed teardown paths consistently.

### 3.8 `test_repeated_create_delete_cycles()`

This test repeatedly creates and destroys the same pair.
It records runtime IDs and task indexes to check reuse behavior.

#### What it proves

- deletion really retires the old runtime
- recreation produces a fresh runtime identity
- task indexes can be reused safely
- cleanup returns the monitor count to zero after each cycle

#### Why it matters

This is the churn test.
It catches bugs that only appear after repeated reuse.

## 4. What the helper tasks do

### Pair client task

The client task:

- identifies its runtime slot
- waits for runtime metadata to settle
- asserts runtime accessor correctness
- sends one message
- waits for the reply
- records the round-trip result

### Pair server task

The server task:

- identifies its runtime slot
- waits for runtime metadata to settle
- asserts runtime accessor correctness
- receives the client message
- replies with the transformed payload

### Local task

The local task:

- validates its runtime context
- records its runtime ID
- checks runtime accessor correctness
- stays alive until the test tears it down

## 5. Runtime identity checks

The tests store runtime IDs for:

- pair tasks
- local tasks

Those IDs are used to prove that a recreated runtime is not just the same stale
object coming back after deletion.

This is important because cleanup bugs often masquerade as "successful"
recreation when the old memory is still being observed.

## 6. Hot/cold snapshot checks in the tests

The tests intentionally verify both data classes:

### Hot values

- CPU cycles
- OE2EL
- signal-related behavior

### Cold values

- task name
- period
- WCET
- execution site

This helps ensure that the monitoring store remains internally consistent.

## 7. Failure-injection strategy

The file defines a weak hook:

- `edge_task_manager_test_hook_should_fail_creation()`

The test harness overrides it to force specific creation failures.
That lets the same test file exercise rollback branches without changing the
production code.

This is the right way to test cleanup logic in embedded code:

- the production code stays real
- the tests inject controlled failure modes
- the cleanup behavior is validated on hardware

## 8. Why the test file is long

It is long because it is intentionally covering the system rather than a single
function.

The task manager is a stateful orchestration layer, so the regression value comes
from observing the whole lifecycle:

- create
- run
- communicate
- update
- snapshot
- destroy
- recreate

## 9. How to read the output

Each assertion prints a `TEST PASSED` or `TEST FAILED` line.
The harness ends by reporting the total pass/fail counts.

That makes the serial output useful as a human-readable regression log.

## 10. Relationship to the Doxygen API reference

For exact function signatures, types, and contracts, see:

- `components/EEAA/include/core/task_manager.h`
- the generated Doxygen pages

Use this markdown page to understand why the tests exist and what each one is
trying to prove.
