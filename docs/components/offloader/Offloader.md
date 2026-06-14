# Offloader Controller

The offloader is the controller layer that decides where a client-side EA task
should route its payload.

It sits on top of the task manager and uses only public snapshots, runtime
accessors, and index-based task-manager mutation helpers.

## 1. What the controller means

An enhanced EA task is split into two cooperating roles:

- a client segment that decides where the work should go
- a server segment that consumes the payload when the route stays local

The offloader does not choose between client and server.
It chooses the route for the client payload:

- `LOCAL` means keep the payload in the local queue path and execute the local
  server half
- `REMOTE` means route the payload to the remote host and bypass the local
  server half

## 2. Scope lock

The first implementation milestone was intentionally narrow:

- one conservative, deterministic routing policy
- client-side candidate selection only
- task-manager-owned mutation of host and execution-site metadata
- no ML model, prediction stack, or external planner

The controller expects explicit, non-empty host labels in its configuration.
It does not silently fall back to the runtime's current host label when a route
is applied.

That scope keeps the controller reviewable and makes the data flow easy to
verify on hardware.

## 3. Information flow

The controller follows one of two flows depending on its configured mode.

Per-task mode:

```text
task_manager snapshots
        |
        v
client-only candidate selection
        |
        v
pure routing policy
        |
        v
route result
        |
        v
task_manager mutation helpers
```

Batch/vector mode:

```text
task_manager snapshots
        |
        v
collect all client candidates
        |
        v
batch policy planning
        |
        v
offloading vector
        |
        v
validate the full vector
        |
        v
task_manager mutation helpers
```

The controller never writes task-manager state directly. It asks the task
manager to update the host label and execution site for the selected task
index or the full planned vector.

## 4. Code layout

The offloader code currently lives in these files:

- `components/EEAA/include/core/offloader.h`
- `components/EEAA/include/core/offloader_policy.h`
- `components/EEAA/include/core/offloader_types.h`
- `components/EEAA/src/core/offloader.c`
- `components/EEAA/src/core/offloader_policy_simple.c`

## 5. Runtime behavior

The controller exposes two levels of operation:

- `edge_offloader_run_for_task_index()` evaluates one known client index
- `edge_offloader_run_once()` scans active tasks, filters client candidates,
  and then follows the configured mode

In per-task mode, the controller evaluates and applies each candidate one at a
time.

In batch/vector mode, the controller collects all active client candidates,
asks the policy to produce a full offloading vector, validates the complete
plan, and then applies the route changes.

The controller uses the task snapshot to decide whether an entry is a client
side candidate.
That keeps the offload decision aligned with the EA pair model.

The control loop cadence is owned by the caller or scheduler. The offloader
uses `control_period_ms` as a configuration hint, but it does not start or
manage its own timer.

The policy result is applied through:

- `edge_task_pair_set_local_host_label_by_index()`
- `edge_task_pair_set_exec_site_by_index()`

Those helpers keep the task manager as the single owner of runtime mutation.

## 6. Policy model

The v1 policy layer is now split into scheduler-aware built-ins plus room for
custom policies.

The default built-in policy is fixed-priority (`FP`). A rate-monotonic (`RM`)
policy is available as a second built-in path and uses the same fixed-
priority foundation but a tighter period-driven decision rule.

EDF is not treated as a built-in FreeRTOS scheduler path. It can still be
represented by a custom policy later, but the controller does not assume it
as an out-of-the-box mode.

The controller does not own schedulability math. It passes candidate data and
the active scheduler model into the selected policy, then applies only
structurally valid results. If the policy rejects a plan, the controller
leaves runtime state unchanged.

The built-in policies fail closed on malformed snapshots. If a candidate is
missing the state required for its policy model, the policy returns an invalid
input failure instead of silently routing the task locally.

The policy layer supports both:

- per-candidate evaluation for compatibility and direct routing helpers
- batch planning for scheduler-aware vector decisions

## 7. Regression coverage

The offloader is covered by two test harness entry points:

- `test/test_offloader.c` for the policy and controller flow
- `test/test_task_lifecycle.c` for the task-manager route mutation helpers and
  their interaction with the existing lifecycle harness

Both harnesses are designed to run on real hardware so the routing path can be
observed end to end.
