# Runtime Observability Test Harness

This harness covers the product-facing runtime diagnostics surface.

The tests verify:

- stopped-runtime diagnostics are safe to query
- route-change events are reported after a controller cycle
- failure events are reported when a policy rejects a plan
- the latest event and counters match the observed controller outcome

The harness is board-backed and uses the public runtime API together with the
public task model. That keeps the observability regression aligned with the
developer surface the runtime facade exposes.
