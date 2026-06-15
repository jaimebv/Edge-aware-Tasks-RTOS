# Hello-World Onboarding Test Harness

`test/test_hello_world_onboarding.c` verifies the shortest FreeRTOS-first path.

## What it covers

- local-first runtime startup and shutdown
- status snapshot reporting for the onboarding labels
- creation of one enriched hello-world pair
- task readiness and cleanup on real hardware
- repeated start/stop reuse of the onboarding runtime path

## Why it exists

The onboarding path is the first thing a new user sees.
This harness keeps that path honest by checking the same runtime and task
contracts that the example uses.

## Runner note

This test module is wired into the lifecycle harness so the board-backed
regression stays end-to-end.
