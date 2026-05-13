# Port-board Test Harness

`test/test_port_board.c` is the regression harness for the EEAA board-layer API.

## What it checks

The test stays abstract to the board implementation while still exercising the
real board contract:

- the reported core ID is in range
- the reported core count is sane
- CPU cycle reads are monotonic over a short window
- `eaPort_Get_Cpu_Cycles_per_ms()` matches `eaPort_Get_Cpu_Freq() * 1000`
- `eaPort_Cycles_to_ms()` converts exact cycle multiples consistently
- a real delay converts to a reasonable millisecond value

## Why the test exists

The board module is the bridge between the portable EEAA layer and the concrete
hardware timing source.

If this layer is wrong, the task manager can still compile but its runtime timing
and monitoring behavior becomes unreliable.

## Output style

The harness uses the shared test helpers so it prints the same:

- `TEST PASSED: <name>`
- `TEST FAILED: <name> (<detail>)`

That keeps the COM log easy to scan during board verification.

## Verification intent

The test is intentionally hardware-backed. It should be run on the target board
and observed from serial output, not treated as a compile-only check.

## Runner note

The current ESP-IDF test firmware keeps a single `app_main` in the lifecycle
harness, which invokes this port-board suite after the lifecycle run finishes.
