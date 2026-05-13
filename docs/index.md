# Edge-aware Tasks RTOS Documentation

Welcome to the project documentation for **Edge-aware Tasks RTOS**.

This repo provides:

- a portable RTOS abstraction layer
- an EEAA task-manager API for paired tasks
- monitoring helpers for snapshots, metrics, and runtime accessors
- board-specific support for the ESP32 target
- runnable examples and hardware-backed tests

## Documentation sources

The API reference is generated from:

- `components/EEAA/include/`
- `components/EEAA/src/`
- `components/EEAA/examples/`
- `src/`

The project-level guides are also included:

- `README.md`
- `CONTRIBUTING.md`

## Generate the docs

```bash
doxygen Doxyfile
```

The HTML output is written to:

```text
docs/doxygen/html/
```

## Documentation policy

- Public headers carry the canonical API reference.
- New or changed public APIs must be documented in Doxygen style.
- Behavior changes must update both code comments and contributor guidance.
- Examples and tests should reflect the documented contract.

## Module map

- `core/task_manager.h` — task lifecycle, runtime accessors, snapshots, cleanup
- `port/port_rtos_freertos.h` — FreeRTOS-backed portable RTOS layer
- `port/port_board_esp32.h` — ESP32-specific board and timing helpers
- `interfaces/platform_interface.h` — platform-neutral timing interface
