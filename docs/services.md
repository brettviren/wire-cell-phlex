# WCT Service Components across PHLEX Nodes

This document describes how WCT "service" components (AnodePlane,
FieldResponse, WireSchemaFile, ColdElecResponse, FftwDFT, etc.) behave
when two or more PHLEX nodes each host a separate WCT `Main` instance,
the specific hazards that arise, and how to control whether services are
shared or kept independent.

---

## Background: the PHLEX node context

Each PHLEX algorithm module (e.g. `wcp_deposet_to_frame`,
`wcp_frame_filter`) owns exactly one `wcphlex::Executor` instance and
therefore exactly one `WireCell::Main` instance.  PHLEX assigns every
module a unique **module label** (the key used in the workflow Jsonnet,
e.g. `frame_sim`, `frame_sigproc`).  The label is injected into the
module config as `module_label` and becomes `m_scope` in the Executor.

`m_scope` drives the names of **boundary nodes** — the
`BoundarySource`/`BoundarySink` objects that connect PHLEX data products
to WCT graph ports — and the Pgrapher instance name:

```
source_name = m_scope + "_frame_source"      e.g. "frame_sim_frame_source"
sink_name   = m_scope + "_frame_sink"        e.g. "frame_sim_frame_sink"
app_name    = m_scope + "_pgrapher"          e.g. "frame_sim_pgrapher"
```

These names are automatically unique per PHLEX node.  **However,
`m_scope` does NOT affect the names of service components** (AnodePlane,
FieldResponse, etc.).  Those names come entirely from the Jsonnet config.

---

## Deferred initialization and its consequences

### Why initialization is deferred

`WireCell::Main::initialize()` is not called in the Executor constructor.
It is called lazily on the **first `operator()` invocation** via
`ensure_initialized()` (DCLP — double-checked locking pattern):

```
Executor constructor
  └─ parse config, register plugins, TLAs, add_app()
     ← NO m_wcmain.initialize() here

First operator() call (event 0)
  └─ ensure_initialized()
       ├─ acquire s_wct_init_mutex   ← global, serializes all WCT inits
       ├─ m_wcmain.initialize()      ← plugins loaded, all configure() calls made
       ├─ initialize_ports()         ← find BoundarySource/BoundarySink
       └─ m_initialized = true
```

The deferral exists because some Executor subclasses need geometry
(a `WireSchema` product) that only becomes available at execution time,
not at module registration time.  `FacadeWireSchema::register_store()`
deposits the geometry in a static map before `ensure_initialized()` is
called, so `AnodePlane::configure()` can find it.

### The serialization mutex

WCT's global `NamedFactoryRegistry` and `PluginManager` are not
thread-safe.  PHLEX may schedule multiple TBB tasks concurrently, so
two Executor instances could reach their first `operator()` call
simultaneously.  `s_wct_init_mutex` (defined in `Executor.cpp`) prevents
concurrent `initialize()` calls.

**Consequence**: if two PHLEX nodes both receive their first event in the
same TBB wave, one will block until the other's `initialize()` completes.
In practice this is a one-time cost per node, not per event.

### Initialization order

Because PHLEX executes a linear pipeline serially (for a single event),
the upstream node always initializes before the downstream node:

```
Event 0:
  frame_sim.operator()(DepoSet)
      └─ ensure_initialized()   ← Sim Main::initialize() runs first
      └─ graph runs → Frame produced

  frame_sigproc.operator()(Frame)
      └─ ensure_initialized()   ← SP Main::initialize() runs second
      └─ graph runs → Frame produced
```

This ordering is deterministic for serial (non-fan-out) pipelines.

### WCT component lifecycle relative to initialization

When `Main::initialize()` runs, it:
1. Loads the Jsonnet config file and evaluates it with the registered TLAs.
2. For each component object in the resulting JSON array:
   a. Calls `Factory::lookup_tn<IComponent>(type, name)` — this creates the
      instance if it does not exist, or returns the existing `shared_ptr` if
      it already does.
   b. Casts to `IConfigurable` and calls `configure(data)`.
3. Starts the Pgrapher application (connects graph edges).

The critical point is step 2a: **the WCT factory is global and
process-wide**.  Two `Main` instances share the same
`NamedFactoryRegistry`.  A component with a given `(type, name)` is
created exactly once; all subsequent lookups return the same
`shared_ptr`.

---

## The global factory and service sharing

### What "service" means here

WCT components fall into two categories for this discussion:

**Boundary nodes** (unique per PHLEX node, always distinct):
- `FrameBoundarySource`, `FrameBoundarySink`, `DepoSetBoundarySource`, etc.
- Named `m_scope + "_frame_source"` etc. — guaranteed unique.

**Service components** (potentially shared across nodes, depends on names):
- `AnodePlane`, `WireSchemaFile`, `FieldResponse`, `ColdElecResponse`,
  `FftwDFT`, `Random`, `PlaneImpactResponse`, `EmpiricalNoiseModel`, etc.
- Named by the Jsonnet config — shared if two configs use the same name.

### What happens when a service is configured twice

When the second `Main::initialize()` looks up `AnodePlane:apa0`:

```
Second Main::initialize()
  Factory::lookup("AnodePlane", "apa0")
    → existing shared_ptr returned     ← same object
  iface->configure(same_data)          ← configure() called AGAIN
```

Whether this is safe depends on the component:

| Component | Re-configure safe? | Notes |
|---|---|---|
| `FftwDFT` | Yes | Empty config; FFTW plans are re-created but numerically identical |
| `WireSchemaFile` | Mostly | Re-reads the same file; tiny float differences possible |
| `FieldResponse` | Mostly | Re-reads the same file; downstream caches already computed |
| `ColdElecResponse` | Yes if params identical | Overwrites waveform; PIRs already cached their copy |
| `AnodePlane` | Mostly | Re-reads wire schema; small geometry float differences possible |
| `PlaneImpactResponse` | Yes | Cached response computed once; not called again at runtime |
| `EmpiricalNoiseModel` | Yes | Channel list computed once |
| `Random` | Yes if seed same | Reseeds RNG; subsequent noise realizations change if seed differs |

**Key finding from testing**: two PHLEX nodes that both use bare
component names (`service_prefix=""`) with identical configurations
produce OmnibusSigProc output that differs by at most ±2 ADC in ≲0.02%
of samples.  The source is floating-point non-reproducibility in
`WireSchemaFile` and `AnodePlane` re-configure (file re-parsing,
geometry recomputation).  This difference is within the noise floor and
is physically insignificant.

---

## Controlling sharing: the `service_prefix` TLA

All PDSP WCT configs in `cfg/` accept a `service_prefix` TLA (default `""`).
Every service component name is formed as `service_prefix + bare_name`:

```jsonnet
function(
    source_name    = "wcphlex_deposet_source",
    sink_name      = "wcphlex_frame_sink",
    app_name       = "wcphlex_pgrapher",
    service_prefix = "",
)

local anode = {
    type: "AnodePlane",
    name: service_prefix + "apa0",   // "apa0" when shared, "sim_apa0" when independent
    ...
};
```

### Shared services (default)

Pass no `service_prefix` (or `service_prefix=""`) to both PHLEX nodes.
All service components use bare names.  The second `Main::initialize()`
finds existing instances in the global factory and re-configures them.

```jsonnet
// sim-sigproc-workflow.jsonnet
frame_sim: {
    cpp: 'wcp_deposet_to_frame',
    wct_config: 'deposet-drift-sim.jsonnet',
    // service_prefix not set → default "" → AnodePlane:apa0, FieldResponse:fr, ...
},
frame_sigproc: {
    cpp: 'wcp_frame_filter',
    wct_config: 'frame-sigproc.jsonnet',
    // service_prefix not set → same bare names → factory returns same instances
},
```

**Use when**: both nodes must see the same physical detector model.
AnodePlane is the canonical example — both sim and sigproc must operate
on the same wire geometry.

**Hazards**:
- Parameters must be consistent across nodes.  If two nodes define the
  same service with different parameters (e.g. different shaping times in
  `ColdElecResponse`), the second configure() silently overwrites the
  first.  Components that cached values from the first configure() will
  see inconsistent state.
- The re-configure introduces ≲±2 ADC rounding differences in
  OmnibusSigProc output (from `WireSchemaFile`/`AnodePlane` re-parsing).

### Independent services

Pass distinct `service_prefix` values to each PHLEX node.  Each node
creates its own set of service instances in the global factory.  The two
sets coexist with no configure() cross-talk.

```jsonnet
// sim-sigproc-mismatch-workflow.jsonnet
frame_sim: {
    cpp: 'wcp_deposet_to_frame',
    wct_config: 'deposet-drift-sim.jsonnet',
    wct_tla: { service_prefix: 'sim_' },
    // → AnodePlane:sim_apa0, FieldResponse:sim_fr, ColdElecResponse:sim_elec, ...
},
frame_sigproc: {
    cpp: 'wcp_frame_filter',
    wct_config: 'frame-sigproc.jsonnet',
    wct_tla: { service_prefix: 'sp_' },
    // → AnodePlane:sp_apa0, FieldResponse:sp_fr, ColdElecResponse:sp_elec, ...
},
```

**Use when**:
- You need different model parameters in sim vs sigproc (e.g. a
  different FieldResponse file to simulate model uncertainty).
- You need provably clean single-configure semantics for each node.
- Multi-APA fan workflows (existing PDHD configs already use this pattern
  with `apa_ident` suffixes: `AnodePlane:apa0`, `AnodePlane:apa1`, etc.).

**Properties**:
- No re-configure: each service is configured exactly once.
- No ±2 ADC rounding artifact.
- Two instances of `FftwDFT`, `WireSchemaFile`, etc. exist concurrently
  in the global factory.  Memory overhead is proportional to the number
  of independent sets.

### Selective sharing (future)

The current `service_prefix` TLA applies a single prefix to all service
components.  Finer control — share some services (e.g. AnodePlane) but
not others (e.g. FieldResponse) — can be achieved by extending the
Jsonnet configs with per-component name TLAs:

```jsonnet
function(
    service_prefix = "",
    fr_name        = service_prefix + "fr",   // override for FR specifically
    anode_name     = service_prefix + "apa0", // override for AnodePlane specifically
)
```

This is straightforward to add when the use case arises.

---

## SP filter objects: always shared

`OmnibusSigProc` looks up 13 filter objects by **hard-coded names**
(`"ROI_tight_lf"`, `"Gaus_tight"`, `"Wiener_tight_U"`, etc.) in C++.
These names come from `cfg/sp-filters.jsonnet` and are not prefixed by
`service_prefix`.  Multiple `OmnibusSigProc` instances (whether from
different PHLEX nodes or different APAs) always share the same filter
objects.

This is safe: `HfFilter::filter_waveform()` and `LfFilter::filter_waveform()`
are `const` methods that operate on local (stack-allocated) output
variables.  There is no shared mutable state, so concurrent calls from
multiple TBB threads are harmless.

---

## Practical guidelines

| Scenario | Recommended approach |
|---|---|
| Single-APA sim + sigproc, same model | Default `service_prefix=""` on both nodes |
| Single-APA sim + sigproc, different FR for model-uncertainty study | `service_prefix="sim_"` / `service_prefix="sp_"` |
| Multi-APA fan-out (N APAs, same geometry type) | Per-APA suffix in Jsonnet (as in `pdhd-apa-sim.jsonnet`); no `service_prefix` TLA needed |
| Concurrent TBB parallel nodes sharing geometry | Shared mode is safe; `s_wct_init_mutex` serializes initialization; post-init usage is read-only per node |
| Need bit-identical output from shared vs independent | Use independent mode (`service_prefix` set); shared mode has ≲±2 ADC rounding artifact |

---

## Future: moving WCT initialization earlier

The deferred initialization is currently necessary because geometry
resources (`WireSchema`) arrive as PHLEX job-layer products unavailable
at module registration time.  If WCT "service" components can be
configured via **PHLEX resources** (available before any events are
processed, in a single-threaded context), `Main::initialize()` could
be called at construction time:

- `s_wct_init_mutex` would no longer be needed.
- Service components would be created before any `operator()` calls,
  eliminating concurrent-initialize races.
- The double-configure hazard would not arise because all service
  components would be configured in a single, ordered pass.

The `FacadeWireSchema` pattern (static registry bridging PHLEX
job-layer delivery to WCT configure-time lookup) is the prototype
for this direction.  See `docs/executors.md` for the lifecycle details
and `wire_cell_phlex/FacadeWireSchema.{hpp,cpp}` for the implementation.
