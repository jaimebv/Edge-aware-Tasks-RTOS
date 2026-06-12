# Offloader Test Harness

`test/test_offloader.c` covers the offloader policy and controller flow.

## What it covers

- null-input handling for the policy interface
- conservative local-first routing
- remote routing when the snapshot exceeds the baseline threshold
- offloader initialization and shutdown
- client-only candidate collection
- route application through the task manager
- LOCAL and REMOTE updates for a paired EA task

## Structure

The harness is split into two suites:

- a policy suite that exercises the pure routing policy
- a controller suite that creates a paired EA task and verifies the controller
  can discover and route the client segment

## Why it matters

The offloader is only useful if the decision path matches the runtime data
model.
This harness checks the policy layer and the controller layer together so the
route choice can be validated against the task-manager metadata seen at runtime.
