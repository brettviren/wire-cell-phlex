# Future development ideas

## Reducing redundancy between code and config

In the current design the user must configure PHLEX to use a WCPh `Executor` (e.g.
a `FrameFilter`) and a WCT graph that is bookended by a `BoundarySource` and
`BoundarySink` of the matching type (e.g. `FrameBoundarySource` and
`FrameBoundarySink`).  A type mismatch produces a runtime error.

A concrete `Executor` "knows" which boundary converter types it uses.  It could
automatically inject boundary-node components into the WCT component list before
`initialize()`, removing the requirement for users to include them explicitly in
their Jsonnet config.  This would require a way for the executor to amend the WCT
component list prior to finalization — something `WireCell::Main` does not currently
support directly.

## Geometry service evolution

The current `wcp_wire_schema_source` / `FacadeWireSchema` pattern bridges a WCT
wire-geometry file into PHLEX's job-layer product system.  In the future, PHLEX
is expected to provide a first-class geometry service at the framework level.  When
that arrives, `FacadeWireSchema` should be extended or replaced:

- The `register_store()` static-map mechanism was designed with this evolution in
  mind: a future executor overload could accept a richer PHLEX geometry product and
  populate the same static map, leaving the WCT-side `FacadeWireSchema` unchanged.
- The `WireSchemaValidator` WCT component and its `configure()`-time factory lookup
  provide a useful regression harness for this transition.

**Resolved:** The original concern that `wire_schema_source` re-created a new
`WireSchema` object on every call is addressed: `wcp_wire_schema_source` uses
PHLEX's job-layer `provide()`, which is called only once per job.  The
`FacadeWireSchema` bridge (Step 9) addresses the configure-time service lifecycle
mismatch described below.

## WCT "service" components as facades over PHLEX resources

WCT has "service" components and PHLEX has "resource" components.  They are similar
in that they are long-lived and shared by multiple DFP nodes.  They differ in that
WCT services are looked up at **configure time** while PHLEX resources are delivered
at **execution time**.

The concrete example chain:
- `WireSchemaFile` (`IWireSchema`): loads a wires JSON file in `configure()`.
- `AnodePlane` (`IAnodePlane`): fetches `IWireSchema` via WCT factory in `configure()`.
- Any `INode`: fetches `IAnodePlane` via WCT factory in `configure()`.

**Resolved (Step 9):** The `FacadeWireSchema` component + deferred executor
initialization (`ensure_initialized()`) bridge these two timelines:

1. PHLEX delivers geometry as a job-layer product.
2. On the first event, the executor calls `FacadeWireSchema::register_store()` to
   deposit the store in a static map, then calls `initialize()`.
3. During `initialize()`, `FacadeWireSchema::configure()` reads from the static map.
4. WCT nodes that look up `IWireSchema` during their own `configure()` calls find
   the `FacadeWireSchema` instance already populated.

**Open:** The same pattern is not yet implemented for `IAnodePlane` or other
configure-time services.  Each would need a corresponding `FacadeXxx` component.

## `IAnodePlane` and other configure-time services

WCT components that require `IAnodePlane`, `IChannelNoiseDB`, or similar
configure-time services can be bridged using the same `FacadeWireSchema` pattern:

1. Define a PHLEX job-layer product type for the relevant geometry data.
2. Create a `FacadeXxx` WCT component (`IXxx + IConfigurable`) with a static
   registry.
3. Add a geometry-aware executor overload that populates the registry before
   `ensure_initialized()`.

## BoundarySource: deque vs single slot

The current `BoundarySource` holds a single slot (fill-once, serve-once, then EOS).
If a WCT graph node produces multiple outputs per input (e.g. splitting one DepoSet
into several frames), `BoundarySink` would need to buffer multiple items.  A
`std::deque`-based implementation would handle this more robustly.  The current
single-slot design is sufficient for all present use cases.

## DepoSetToFrame: geometry-aware overload

`DepoSetToFrame` does not yet have an `operator()(WireSchema const&, DepoSet const&)`
overload analogous to `FrameFilter`'s geometry-aware path.  Adding it requires the
same pattern: consume a job-layer WireSchema, call `register_store()`, then
`ensure_initialized()`.  The executor infrastructure (static mutex, deferred init)
is already in place.

## CMake install: PHLEX_PLUGIN_PATH hint

After `cmake --install`, downstream users need to add the install `lib/` directory
to `PHLEX_PLUGIN_PATH`.  A small CMake helper or pkg-config fragment that exports
this path would improve the out-of-the-box experience.
