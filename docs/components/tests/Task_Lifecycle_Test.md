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

The lifecycle harness is isolated from the board harness.
Run `test/test_task_lifecycle.c` on its own when validating task-manager
changes; run `test/test_port_board.c` separately for PORT changes.
