# Edge-aware Tasks RTOS Documentation Hub

This documentation set is the user-facing and contributor-facing reference for the project.
It is intentionally broader than the API reference: it explains the architecture,
configuration, module responsibilities, test strategy, and the way the pieces fit together.

## Start here

- [System docs](system/index.md)
- [Component docs](components/index.md)
- [System Architecture](system/System_Architecture.md)
- [System Configuration](system/SystemConfig.md)
- [Task Management](components/tasks/Task_Management.md)
- [Port Layer](components/port/Port.md)
- [Port-RTOS Test Harness](components/tests/Port_Rtos_Test.md)
- [Task Manager Tests](components/tests/task_manager.md)

## What this documentation is for

- help users understand what the framework does
- help contributors change the system without breaking its architecture
- show the information flow across modules
- explain the production assumptions behind the task manager, ports, and tests
- act as the high-level companion to the Doxygen API reference

## How the repository documentation is organized

- **Doxygen reference**: generated from headers and module comments with `doxygen Doxyfile`
- **System docs**: long-form explanations of architecture and configuration
- **Component docs**: detailed module guides for task management, ports, and tests
- **Contributor docs**: rules for style, workflow, and documentation discipline

## Recommended reading order

1. [System Architecture](system/System_Architecture.md)
2. [System Configuration](system/SystemConfig.md)
3. [Port Layer](components/port/Port.md)
4. [Port-RTOS Test Harness](components/tests/Port_Rtos_Test.md)
5. [Task Management](components/tasks/Task_Management.md)
6. [Task Manager Tests](components/tests/task_manager.md)
7. Doxygen API reference generated from the public headers

## Notes for maintainers

- When public behavior changes, update the markdown docs and the Doxygen comments together.
- Keep examples aligned with the docs.
- Prefer precise technical writing over marketing language.
- Treat these pages as living documentation: they should describe the current implementation, not an aspirational design.
