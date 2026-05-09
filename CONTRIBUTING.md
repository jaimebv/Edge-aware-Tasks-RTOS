# Contributing

Thanks for taking the time to help improve this project.

This repository is built to stay readable, hardware-safe, and easy to review. We welcome thoughtful contributions and we value steady, well-tested work over rushed changes.

## Before you start

- Open an issue before starting larger work, especially for new APIs, behavior changes, or board support.
- Keep changes focused. One pull request should solve one clear problem.
- If you are unsure whether a change belongs in code, tests, or documentation, ask first.

## Branching and pull requests

- Create a dedicated branch for each contribution.
- Use a clear branch name that matches the work.
- Keep commits small and logical.
- Avoid mixing refactors, bug fixes, and feature work in the same PR unless there is a strong reason.
- Write PR descriptions that explain:
  - what changed
  - why it changed
  - how it was tested
  - any hardware notes or limitations

## Required workflow

Every change should follow this order:

1. Understand the current code path.
2. Implement the smallest useful change.
3. Add or update tests.
4. Build locally.
5. Flash the board.
6. Run the code on the board and verify the serial output.
7. Open the pull request only after all of the above are clean.

If a change cannot be built and tested on the board, it is not ready yet.

## Testing rules

- Every new API must have a corresponding test in the `test/` folder.
- Follow the existing naming style and place the coverage in the matching `test_<module>.c` file whenever possible.
- If a module does not yet have a matching test file, create one rather than hiding the coverage inside unrelated code.
- Keep tests close to the behavior they validate.
- Prefer hardware-backed verification for task creation, cleanup, queue flow, timing, and monitoring logic.

### Minimum bar for code changes

For any committed code:

- it must compile locally
- it must upload to the board successfully
- it must run on the board
- it must produce the expected serial output or test result

## Code style

- Keep the code clear and straightforward.
- Prefer explicit names over clever shortcuts.
- Keep RTOS abstractions clean and portable.
- Do not expand `src/main.c` with one-off experiments or long-lived test harnesses.
- When a change affects `main.c`, keep the entrypoint stable and move validation logic into tests or examples when possible.
- Avoid large mechanical edits unless they are part of the actual fix.

## API and architecture guidance

- Preserve compatibility unless an API change is truly necessary.
- If you change a public header, update the implementation, tests, and any affected example.
- If you add or change task-manager behavior, make sure the task lifecycle, ownership, and cleanup paths are covered.
- Keep the boundary between `port/`, `core/`, `examples/`, and `test/` clear.
- Prefer small, predictable APIs over broad ones.

## Documentation expectations

Update documentation when behavior changes.

At minimum, update the relevant docs when you change:

- a public API
- a task lifecycle rule
- a board requirement
- build or flash steps
- supported usage patterns

If the project adds a new workflow, document it in plain language.

## Generated files and noise

- Do not commit build artifacts.
- Do not commit unrelated tool-generated file churn.
- If a build tool changes a lockfile, config, or generated file that is not part of the intended change, call it out in the PR.

## Hardware verification

This project is for embedded work, so hardware matters.

Before merging code that touches runtime behavior:

- verify the build on the board
- flash the firmware
- check the serial monitor
- confirm the task flow or API behavior you changed

## Review mindset

We review for correctness first, then simplicity, then polish.

Good contributions are:

- easy to understand
- backed by tests
- verified on hardware
- limited in scope
- documented where needed

If you can make the change smaller, do that.
