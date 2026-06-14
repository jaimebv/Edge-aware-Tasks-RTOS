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
- `edge_task_spec_init_enriched()`
- `edge_task_spec_init_local()`
- `edge_task_spec_set_local_host_label()`
- `edge_task_spec_set_deadline_ms()`
- `edge_task_spec_set_wcet()`

These helpers are intentionally thin. They fill the standard defaults while
still forwarding to the same underlying runtime and task-manager code.

That separation keeps the task model declarative and lets the runtime facade
own lifecycle control.
