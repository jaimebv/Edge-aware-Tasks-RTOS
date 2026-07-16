# Runtime Facade

The runtime facade is the v1 developer entry point for lifecycle control and
runtime observation.

It sits above the task manager and offloader so application code can work with
one stable surface:

1. configure the runtime
2. start the runtime
3. create task pairs through the public task model
4. query runtime status
5. run one controller cycle
6. stop the runtime

## Public contract

The facade is implemented by `api/runtime.h` and backed by the existing task
manager and offloader modules.

The main responsibilities are:

- provide a default runtime configuration
- start the task manager and controller together
- expose a board-safe status snapshot
- forward controller ticks to the offloader
- stop the controller cleanly

## Information flow

```text
application code
    -> api/runtime.h
    -> api/runtime.c
    -> task_manager + offloader
    -> status snapshot / controller state
```

This keeps application code away from runtime ownership details while still
allowing tests and examples to exercise the real runtime path.

## Relationship to the public task model

The runtime facade does not replace the public task model.
It complements it:

- `edge_task_spec_t` still describes the task pair
- `CreateEATaskFromSpecEx()` still creates the runtime objects
- `edge_runtime_start()` activates the surrounding runtime services
- `edge_runtime_status()` reports the aggregate state

## Happy path helpers

The v1 surface also exposes small convenience wrappers so common application
code does not have to assemble a full runtime config by hand:

- `edge_runtime_start_default()`
- `edge_runtime_start_local_first()`
- `edge_runtime_config_set_scheduler_policy()`
- `edge_runtime_config_set_policy()`
- `edge_task_spec_init_enriched()`
- `edge_task_spec_init_local()`
- `edge_task_spec_set_local_host_label()`
- `edge_task_spec_set_deadline_ms()`
- `edge_task_spec_set_wcet()`

The runtime status snapshot also exposes the selected scheduler policy so
tests and applications can confirm whether the default fixed-priority path or
the RM policy path is active.

The runtime diagnostics snapshot returned by `edge_runtime_diagnostics()`
extends that view with observability data from the offloader:

- route-change counters
- failure counters
- the latest route or failure event

That makes it possible to inspect what the controller did without reaching
into private controller state.

If no explicit policy override is supplied, the runtime falls back to the
scheduler-selected built-in policy. In v1, EDF and custom scheduler markers
still resolve to the fixed-priority default unless a custom policy descriptor
is provided.

These helpers are intentionally thin. They fill the standard defaults while
still forwarding to the same underlying runtime and task-manager code.

## Runnable demos

The board application in `src/` now exposes two switchable demos:

- `EA_EXAMPLE_HAPPY_PATH` builds the short convenience-flow sample.
- `EA_EXAMPLE_ADVANCED_API` builds the full explicit contract sample using the
  public task-spec helpers and runtime accessors.

The default `nodemcu-32s` build falls back to the advanced demo, while the
`nodemcu-32s-happy` PlatformIO environment compiles the happy-path demo.

That separation keeps the task model declarative and lets the runtime facade
own lifecycle control.
