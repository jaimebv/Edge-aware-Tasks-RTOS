# Task Lifecycle Test Harness

`test/test_task_lifecycle.c` is the main embedded regression harness for the
EEAA task manager.

## What it covers

- null and invalid API paths
- rollback behavior for partial task creation failures
- queue-allocation failure handling
- single local-task creation
- live pair-task soak batches
- snapshot correctness after metric updates
- cleanup branches for client-only and full-pair teardown
- repeated create/delete churn to expose reuse bugs

## Output style

The test file uses the shared helpers in `test/test_helpers.c` so every case
prints the same pass/fail format.

That makes the serial log easy to scan during board verification.

## Why it matters

This harness proves that the task manager behaves correctly on real hardware,
not just at compile time.

## Runner note

On this branch the lifecycle harness also invokes the board test suite after it
prints its own summary so the ESP-IDF test firmware can keep a single app entry
point while still exercising the new `test/test_port_board.c` coverage.
