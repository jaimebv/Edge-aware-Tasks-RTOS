# Runtime API Test Harness

This harness covers the product-grade runtime facade.

The tests verify:

- default runtime startup and shutdown
- status snapshot reporting
- controller forwarding through the runtime facade
- invalid runtime configuration handling

The runtime observability coverage lives in
`test/test_runtime_observability.c` and verifies:

- diagnostics snapshot reporting
- route-change events
- failure events
- null-safe and stopped-runtime diagnostics queries

The harness is board-backed and uses the public runtime API together with the
public task model. That keeps the regression close to the developer experience
the facade is meant to support.
