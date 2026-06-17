# Edge-aware Tasks RTOS

Edge-aware Tasks RTOS is an ESP32 / ESP-IDF project for modeling, creating, and observing task pairs with a portable RTOS abstraction layer.

It is centered on the `EEAA` component, which wraps FreeRTOS primitives behind a stable interface and adds task metadata, monitoring helpers, and client/server orchestration utilities.

## What it provides

- **Portable RTOS abstraction** for tasks, queues, mutexes, delays, and suspend/resume flows
- **Edge-task modeling** for local, enriched, and remote execution styles
- **Task monitoring metadata** for host, relation, core, period, WCET, latency, and runtime tracking
- **Client/server task pairing** with queue-based message exchange
- **Runtime facade** for product-grade start/stop/status control over the task manager and offloader
- **Reusable examples** showing task creation, scheduling, synchronization, memory management, and runnable demo flows

## How it works

The project builds a thin abstraction on top of FreeRTOS:

- `port/port_interface_types.h` defines RTOS-agnostic handles and status codes.
- `port/port_rtos_freertos.c` maps those abstractions to native FreeRTOS APIs.
- `core/task_manager.c` creates and tracks edge-aware task pairs, including queues and per-task metadata.
- `api/runtime.c` wraps the task manager and offloader behind a product-grade runtime facade.
- `src/examples/` demonstrates the runnable board demos.

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

### Quickstart

If you want the shortest first run, start here:

- [FreeRTOS First Onboarding Quickstart](docs/getting-started/Quickstart.md)
- [Contributor Setup](docs/getting-started/Contributor_Setup.md)

### Build

```bash
pio run -d . -e nodemcu-32s
```

To build the selectable demos:

```bash
pio run -d . -e nodemcu-32s-happy
pio run -d . -e nodemcu-32s-advanced
pio run -d . -e nodemcu-32s-hello
```

### Flash

```bash
pio run -d . -e nodemcu-32s -t upload
```

Use the matching environment name when flashing one of the selectable demos.

### Hello-world onboarding

The `nodemcu-32s-hello` environment is the shortest board-backed first run.
It starts the runtime with the local-first helper, creates one enriched task
pair, and prints a compact hello/heartbeat serial trace.

### Monitor

```bash
pio device monitor -p /dev/ttyUSB0 -b 115200
```

## Support and contribution

- Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.
- Use the GitHub issue tracker for bugs and feature requests.
- For security-sensitive reports, use the private advisory flow described in [SECURITY.md](SECURITY.md).
- For release history and versioning expectations, see [CHANGELOG.md](CHANGELOG.md).

## Documentation

Read the documentation hub in /docs:

## Example entry points

- `components/EEAA/examples/tasks/ea_task_creation_example.c` — dynamic edge-task pair creation
- `components/EEAA/examples/api/ea_runtime_api_example.c` — runtime facade usage flow
- `components/EEAA/examples/port/port_rtos_example.c` — RTOS abstraction demos
- `components/EEAA/examples/port/port_board_example.c` — board abstraction demo
- `src/examples/happy_path_example.c` — selectable happy-path runtime/task demo
- `src/examples/advanced_api_example.c` — selectable advanced runtime/task demo
- `src/examples/hello_world_example.c` — selectable FreeRTOS-first onboarding demo


## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.


  
## License

See `LICENSE` for licensing details.
