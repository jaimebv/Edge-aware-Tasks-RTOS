# Edge-aware Tasks RTOS

Edge-aware Tasks RTOS is an ESP32 / ESP-IDF project for modeling, creating, and observing task pairs with a portable RTOS abstraction layer.

It is centered on the `EEAA` component, which wraps FreeRTOS primitives behind a stable interface and adds task metadata, monitoring helpers, and client/server orchestration utilities.

## What it provides

- **Portable RTOS abstraction** for tasks, queues, mutexes, delays, and suspend/resume flows
- **Edge-task modeling** for local, enriched, and remote execution styles
- **Task monitoring metadata** for host, relation, core, period, WCET, latency, and runtime tracking
- **Client/server task pairing** with queue-based message exchange
- **Runtime facade** for product-grade start/stop/status control over the task manager and offloader
- **Reusable examples** showing task creation, scheduling, synchronization, and memory management

## How it works

The project builds a thin abstraction on top of FreeRTOS:

- `port/port_interface_types.h` defines RTOS-agnostic handles and status codes.
- `port/port_rtos_freertos.c` maps those abstractions to native FreeRTOS APIs.
- `core/task_manager.c` creates and tracks edge-aware task pairs, including queues and per-task metadata.
- `api/runtime.c` wraps the task manager and offloader behind a product-grade runtime facade.
- `examples/` demonstrates how to use the layer in real flows.

The main task model uses two cooperating roles:

- **Client**: performs local work, sends a request, then waits.
- **Server**: receives the request, processes it, and replies.

That structure makes the repo useful for studying edge-oriented workloads, deadline-aware task design, and RTOS coordination patterns.

## Repository layout

- `components/EEAA/include/` — public headers
- `components/EEAA/src/` — component implementation
- `components/EEAA/examples/` — runnable usage examples
- `docs/` — long-form system, component, and test documentation
- `src/main.c` — application entrypoint
- `platformio.ini` — PlatformIO environment configuration

## Supported target

- **Board:** `nodemcu-32s`
- **Framework:** `espidf`
- **Platform:** `espressif32`

## Getting started

### Build

```bash
pio run -d . -e nodemcu-32s
```

### Flash

```bash
pio run -d . -e nodemcu-32s -t upload
```

### Monitor

```bash
pio device monitor -p /dev/ttyUSB0 -b 115200
```

## Documentation

Read the documentation hub in /docs:

## Example entry points

- `components/EEAA/examples/tasks/ea_task_creation_example.c` — dynamic edge-task pair creation
- `components/EEAA/examples/api/ea_runtime_api_example.c` — runtime facade usage flow
- `components/EEAA/examples/port/port_rtos_example.c` — RTOS abstraction demos
- `components/EEAA/examples/port/port_board_example.c` — board abstraction demo


## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.


  
## License

See `LICENSE` for licensing details.
