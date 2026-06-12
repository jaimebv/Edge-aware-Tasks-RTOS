# Offloader Scope Lock

This note records the Phase 0 contract for the offloader work in `issue-34`.

## Controller semantics

- The controller operates on **client segments only**.
- A routing decision does **not** choose between client and server tasks.
- A routing decision chooses between:
  - `LOCAL`: use the existing local client-to-server queue path and let the local server half execute.
  - `REMOTE`: send the payload to the remote host and bypass the local server half.

## Scope

- The first milestone will use one conservative, deterministic policy.
- The controller will consume task-manager snapshots and runtime accessors only.
- The task manager remains the only module allowed to mutate runtime host and execution-site state.
- No prediction model, ML model, or network planner is part of Phase 0 or Phase 1.

## Implementation intent

- Keep the offloader thin.
- Keep the policy pure.
- Keep candidate selection limited to client-side EA tasks.
- Preserve the current task-manager ownership and cleanup rules.
