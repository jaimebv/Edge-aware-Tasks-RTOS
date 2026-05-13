# Edge-aware Tasks RTOS

Edge-aware Tasks RTOS is an ESP32 / ESP-IDF project for modeling, creating, and observing task pairs with a portable RTOS abstraction layer.

It is centered on the `EEAA` component, which wraps FreeRTOS primitives behind a stable interface and adds task metadata, monitoring helpers, and client/server orchestration utilities.

## What it provides

- **Portable RTOS abstraction** for tasks, queues, mutexes, delays, and suspend/resume flows
- **Edge-task modeling** for local, enriched, and remote execution styles
- **Task monitoring metadata** for host, relation, core, period, WCET, latency, and runtime tracking
- **Client/server task pairing** with queue-based message exchange
- **Reusable examples** showing task creation, scheduling, synchronization, and memory management

## How it works

The project builds a thin abstraction on top of FreeRTOS:

- `port/port_interface_types.h` defines RTOS-agnostic handles and status codes.
- `port/port_rtos_freertos.c` maps those abstractions to native FreeRTOS APIs.
- `core/task_manager.c` creates and tracks edge-aware task pairs, including queues and per-task metadata.
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

Read the human-facing documentation hub:

- `docs/index.md`
- `docs/system/System_Architecture.md`
- `docs/system/SystemConfig.md`
- `docs/components/tasks/Task_Management.md`
- `docs/components/port/Port.md`
- `docs/components/tests/task_manager.md`

Generate the API reference with Doxygen:

```bash
doxygen Doxyfile
```

The generated HTML lives in `docs/doxygen/html/`.

## Example entry points

- `components/EEAA/examples/tasks/ea_task_creation_example.c` — dynamic edge-task pair creation
- `components/EEAA/examples/port/port_rtos_example.c` — RTOS abstraction demos
- `components/EEAA/examples/port/port_board_example.c` — board abstraction demo

## Working with the task manager

The task manager provides helpers for:

- creating task pairs with shared communication queues
- recording task identity and relationship metadata
- capturing task snapshots for monitoring and analysis
- using a consistent naming and execution-site model across tasks

## Documentation

For higher-level guidance, start with the markdown pages in `docs/`:

- `docs/index.md`
- `docs/components/tests/index.md`
- `docs/components/tests/Shared_Test_Helpers.md`
- `docs/components/tests/Task_Lifecycle_Test.md`

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.

### Suggested workflow

1. Open an issue for a bug, idea, or documentation gap.
2. Create a focused branch.
3. Keep changes small and reviewable.
4. Update code and docs together when behavior changes.
5. Verify builds before opening a pull request.

### Coding standards

- Prefer clear, explicit task and component names.
- Keep abstractions small and readable.
- Avoid introducing API changes without updating examples and docs.
- Preserve the RTOS-agnostic boundaries between `port/`, `core/`, and `examples/`.

### Documentation standard

- Use Doxygen block comments for every public function, struct, enum, and typedef.
- Start each public item with a one-line `@brief`.
- Document parameters with `@param[in]`, `@param[out]`, or `@param[in,out]` as appropriate.
- Document return values with `@return`.
- Use `@note`, `@warning`, `@deprecated`, and `@see` when they add real value.
- Keep public-header comments as the source of truth; update them whenever behavior changes.
- Regenerate the Doxygen output whenever public APIs, contracts, or examples change.

## License

See `LICENSE` for licensing details.
