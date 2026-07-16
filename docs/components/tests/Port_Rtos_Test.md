# Port-RTOS Test Harness

`test/test_port_rtos.c` is the regression harness for the EEAA RTOS abstraction layer.

## What it checks

The harness stays abstract to the concrete RTOS backend while validating the
portable contract:

- null-safe queue and lookup guards
- null-safe task-info guards
- memory allocation and tick/delay helpers
- current-task inspection
- task creation and handle lookup by name
- task-state translation through `eaPort_Get_Task_Info()`
- queue send/receive round-trips
- mutex-protected shared state updates
- suspend/resume and teardown cleanup paths

## Why the test exists

The RTOS port is the contract between the task manager and the scheduler backend.
If this layer drifts, higher layers may still compile but runtime behavior becomes
unreliable.

## Output style

The harness uses the shared test helpers so it prints the same format as the
other embedded tests:

- `TEST PASSED: <name>`
- `TEST FAILED: <name> (<detail>)`

## Verification intent

This test is hardware-backed. It should be run on the ESP32 board and checked
through serial output.

## Runner note

The port-RTOS harness is isolated from the board and task-lifecycle harnesses.
Run `test/test_port_rtos.c` by itself when validating RTOS-port changes.
