# Shared Test Helpers

The `test/` folder now has a reusable helper module:

- `test/test_helpers.c`
- `test/test_helpers.h`

## Why it exists

The project prints test progress in a very specific format:

- `TEST PASSED: <name>`
- `TEST FAILED: <name> (<detail>)`

Keeping that behavior in one shared helper avoids copy/paste drift across future
embedded test files.

## Current helpers

- `test_helpers_reset_counts()` resets the shared pass/fail counters
- `pass()` prints a success line and increments the pass count
- `fail()` prints a failure line and increments the fail count
- `expect_true()` combines a boolean check with the standard pass/fail output
- `test_helpers_pass_count()` returns the current pass count
- `test_helpers_fail_count()` returns the current fail count

## Intended usage

Future test files should include `test_helpers.h` and use these helpers instead
of defining local copies of the same output logic.

That keeps the format stable and makes new tests easier to read.
