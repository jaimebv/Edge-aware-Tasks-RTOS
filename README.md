# Edge-aware Tasks RTOS

ESP32 / ESP-IDF project for edge-aware task management and RTOS abstraction.

## What this repo is

This repository contains the `EEAA` component, which provides:
- a portable RTOS abstraction layer
- task creation and task-monitoring helpers
- queue, mutex, delay, and suspend/resume wrappers
- example flows for task creation and RTOS usage

## Project layout

- `components/EEAA/include/` - public headers
- `components/EEAA/src/` - implementation
- `components/EEAA/examples/` - runnable examples
- `src/main.c` - app entrypoint (currently minimal)

## Main ideas

The code is organized around edge-aware tasks that can be:
- local
- enriched
- remote

The task manager keeps track of task metadata such as name, host, relation, execution site, period, WCET, and other monitoring fields.

## Build target

- Board: `nodemcu-32s`
- Framework: `espidf`
- Platform: `espressif32`

## Build / flash / monitor

```bash
pio run -d . -e nodemcu-32s
pio run -d . -e nodemcu-32s -t upload
pio device monitor -p /dev/ttyUSB0 -b 115200
```

## Examples

Useful example entry points:
- `components/EEAA/examples/tasks/ea_task_creation_example.c`
- `components/EEAA/examples/port/port_rtos_example.c`
- `components/EEAA/examples/port/port_board_example.c`

## Notes

- `src/main.c` is currently empty, so the repo behaves more like a component library + examples than a finished app.
- The repo would benefit from a fuller architecture doc, task flow diagram, and a short usage guide for the task manager APIs.
