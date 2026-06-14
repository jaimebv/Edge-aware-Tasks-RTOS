# Runtime API Test Harness

This harness covers the product-grade runtime facade.

The tests verify:

- default runtime startup and shutdown
- status snapshot reporting
- controller forwarding through the runtime facade
- invalid runtime configuration handling

The harness is board-backed and uses the public runtime API together with the
public task model. That keeps the regression close to the developer experience
the facade is meant to support.
