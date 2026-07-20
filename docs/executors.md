# Executor Design

This document describes the `wcphlex::Executor` class hierarchy in
`wire_cell_phlex/Executor.{hpp,cpp}` and gives a step-by-step guide for
adding new subclasses.

## Overview

Each `Executor` subclass wraps a `WireCell::Main` instance and a pair
(or more) of **boundary nodes** — `BoundarySource<I>` and/or
`BoundarySink<I>` — that act as the entry and exit points of a
persistent WCT Pgrapher sub-graph.  PHLEX drives the sub-graph by
calling `operator()` once per event.

The PHLEX module file (e.g. `modules/frame_filter.cpp`) constructs one
`Executor` subclass instance, then calls `operator()` in its `execute()`
method.  The `Executor` hides all WCT lifecycle management from the
module.

## Architecture

```
PHLEX module file                  Executor subclass           WCT Pgrapher graph
──────────────────────────────────────────────────────────────────────────────────
execute(event) {               operator()(Frame input) {
  Frame f = frame_from_event   1. ensure_initialized()    m_wcmain.initialize()  ←─ once
  result = executor(ws, f);       initialize_ports()         Factory::find …
}                              2. m_source->fill(f.ptr)   BoundarySource::fill()
                               3. run_graph()              m_wcmain()  [Pgraph tick]
                               4. return sink->drain()     BoundarySink::drain()
                               }
```

The same Pgrapher graph is re-driven event after event: `BoundarySource`
holds a queue, `fill()` enqueues one item, `run_graph()` / `m_wcmain()`
ticks the graph until quiescent (one item dequeued, processed, output
enqueued in `BoundarySink`), and `drain()` pops the output.

This pattern mirrors `larwirecell` WCLS: `visit(event)` + `m_wcmain()`
+ `visit()`.

## Lifecycle

```
1. Executor() constructor
   ├── m_wcmain.add_config(wct_config)
   ├── m_wcmain.add_plugin("wire_cell_phlex")  (always)
   ├── m_wcmain.add_plugin(...)  for each entry in wct_plugins
   ├── m_wcmain.tla_var(k, v)   for each entry in wct_tla
   ├── m_scope = module_label (from PHLEX config, e.g. "sigproc_a")
   ├── m_app_name = m_scope + "_pgrapher"
   ├── m_wcmain.tla_var("app_name", m_app_name)
   └── m_wcmain.add_app(m_app_type + ":" + m_app_name)

   Subclass constructor (called after base):
   ├── compute m_src_name = m_scope + "_frame_source" (or similar)
   ├── compute m_snk_name = m_scope + "_frame_sink"   (or similar)
   ├── m_wcmain.tla_var("source_name", m_src_name)
   └── m_wcmain.tla_var("sink_name",   m_snk_name)

2. First operator() call → ensure_initialized() [DCLP]
   ├── acquire s_wct_init_mutex  (global, serializes all WCT inits)
   ├── setup_debug_logging()
   ├── m_wcmain.initialize()     (WCT: load plugins, configure components)
   ├── initialize_ports()        (virtual: Factory::find boundary nodes)
   └── m_initialized.store(true, release)

3. Every operator() call
   ├── ensure_initialized()  (fast-path after first call)
   ├── source->fill(ptr)     (one per input boundary)
   ├── run_graph()           (m_wcmain() — tick graph)
   └── sink->drain()         (one per output boundary, if any)

4. Destructor
   └── WireCell::Main::~Main() calls finalize() on all ITerminal components
       (flushes file writers etc.).  Subclass destructors must NOT call
       m_wcmain.finalize() manually — that causes a double-finalize assertion
       in boost::iostreams.
```

### Thread safety

`s_wct_init_mutex` serializes concurrent `m_wcmain.initialize()` calls
because WCT's global `NamedFactoryRegistry` and `PluginManager` are not
thread-safe.  After initialization the boundary nodes are accessed only
by the single PHLEX thread that owns the Executor instance (one instance
per PHLEX module label), so no further locking is needed.

Note: this need for initializing WCT during PHLEX execution time is due to
anticipated need of having WCT "service" components be facades over PHLEX
"resources" which are available at each execution.  If these can be made
available earlier and in a single-thread context, WCT initialization will be
moved there/then.

The `m_initialized` store is placed **after** `initialize_ports()` so
that any thread observing `m_initialized == true` on the acquire fast-path
is guaranteed to see fully-assigned boundary pointers (happens-before).

## Existing subclasses

| Subclass | `operator()` signature | Boundary nodes | WCT graph pattern |
|---|---|---|---|
| `FrameFilter` | `Frame(Frame)` or `Frame(WireSchema, Frame)` | `FrameBoundarySource` + `FrameBoundarySink` | signal processing, pass-through |
| `DepoSetToFrame` | `Frame(DepoSet)` | `DepoSetBoundarySource` + `FrameBoundarySink` | drift + electronics simulation |
| `DepoSetSourceFile` | `DepoSet()` | `DepoSetBoundarySink` (no source) | read depo file; run once, drain per call |
| `DepoSetSinkFile` | `void(DepoSet)` | `DepoSetBoundarySource` (no sink) | write depo file; finalize on destruction |
| `DepoSetFilter` | `DepoSet(DepoSet)` | `DepoSetBoundarySource` + `DepoSetBoundarySink` | drift filter or transform |
| `FrameSourceFile` | `Frame()` | `FrameBoundarySink` (no source) | read frame file; run once, drain per call |
| `FrameSinkFile` | `void(Frame)` | `FrameBoundarySource` (no sink) | write frame file; finalize on destruction |
| `FrameFaninSinkFile` | `void(Frame×4)` | 4 × `FrameBoundarySource` (no sink) | fan-in merge; write merged frame file |

### Subclass categories

**Filter** (`FrameFilter`, `DepoSetToFrame`, `DepoSetFilter`): one source
+ one sink; fill → run → drain on every call.

**Source-from-file** (`DepoSetSourceFile`, `FrameSourceFile`): no boundary
source; on the first call, run the graph to completion (queuing all outputs
in the boundary sink), then drain one item per call.  The `m_graph_ran`
atomic prevents re-running the graph on subsequent calls.

**Sink-to-file** (`DepoSetSinkFile`, `FrameSinkFile`): one boundary source,
no boundary sink; fill → run on every call; WCT finalizes the file writer on
destruction.

**Fan-in sink** (`FrameFaninSinkFile`): N boundary sources, no boundary sink;
fill all N → run on every call; WCT fan-in merges inside the graph before
writing.

---

## How to add a new Executor subclass

This section walks through adding a hypothetical `FrameToDepoSet` subclass
(inverse simulation: digitized frame → depos via some reconstruction step).

### Step 1 — Decide inputs, outputs, and boundary nodes

| Direction | WCT interface | Boundary class | TLA key |
|---|---|---|---|
| Input to graph | `IFrameSource` | `BoundarySource<IFrameSource>` | `source_name` |
| Output from graph | `IDepoSetSink` | `BoundarySink<IDepoSetSink>` | `sink_name` |

### Step 2 — Declare the subclass in `Executor.h`

Add after the last subclass:

```cpp
// ---------------------------------------------------------------------------
// Concrete executor: IFrame → IDepoSet (reconstruction).
//
// WCT boundary node instance names derived from m_scope:
//   source_name = m_scope + "_frame_source"    (FrameBoundarySource)
//   sink_name   = m_scope + "_deposet_sink"    (DepoSetBoundarySink)
//   app_name    = m_scope + "_pgrapher"        (Pgrapher)
// ---------------------------------------------------------------------------
class FrameToDepoSet : public Executor {
public:
    explicit FrameToDepoSet(boost::json::object const& config);

    DepoSet operator()(Frame const& input);

private:
    void initialize_ports() override;

    std::string m_src_name;
    std::string m_snk_name;

    BoundarySource<WireCell::IFrameSource>* m_source{nullptr};
    BoundarySink<WireCell::IDepoSetSink>*   m_sink{nullptr};
};
```

### Step 3 — Implement in `Executor.cpp`

```cpp
// ---------------------------------------------------------------------------
// FrameToDepoSet
// ---------------------------------------------------------------------------

FrameToDepoSet::FrameToDepoSet(boost::json::object const& config)
    : Executor(config)
{
    m_src_name = m_scope + "_frame_source";
    m_snk_name = m_scope + "_deposet_sink";

    m_wcmain.tla_var("source_name", m_src_name);
    m_wcmain.tla_var("sink_name",   m_snk_name);
}

void FrameToDepoSet::initialize_ports()
{
    m_source = find_boundary<WireCell::IFrameSource,
                             BoundarySource<WireCell::IFrameSource>>(
                   "FrameBoundarySource", m_src_name);

    m_sink = find_boundary<WireCell::IDepoSetSink,
                           BoundarySink<WireCell::IDepoSetSink>>(
                 "DepoSetBoundarySink", m_snk_name);
}

DepoSet FrameToDepoSet::operator()(Frame const& input)
{
    ensure_initialized();
    m_source->fill(input.ptr);
    run_graph();
    return DepoSet{m_sink->drain()};
}
```

### Step 4 — Write the WCT Jsonnet config

Create `cfg/frame-to-deposet.jsonnet`:

```jsonnet
// frame-to-deposet.jsonnet
// TLA parameters: source_name, sink_name, app_name, [apa_ident, ...]
function(source_name, sink_name, app_name, apa_ident="0")
{
  local src = {
    type: "FrameBoundarySource",
    name: source_name,
    data: { ... },
  },
  local reco = {
    type: "MyRecoComponent",
    name: "reco_apa" + apa_ident,
    data: { ... },
  },
  local snk = {
    type: "DepoSetBoundarySink",
    name: sink_name,
    data: {},
  },
  sequence: [src, reco, snk],
  edges: [
    { tail: {node: src.name},  head: {node: reco.name} },
    { tail: {node: reco.name}, head: {node: snk.name}  },
  ],
  apps: [{
    type: "Pgrapher",
    name: app_name,
    data: { edges: self.edges },
  }],
}
```

**Rules for Jsonnet configs:**

- The Jsonnet function must accept at minimum `source_name`, `sink_name`,
  and `app_name` as TLA parameters (matching the TLAs registered in the
  constructor).  Subclasses that only have a source or only have a sink
  should omit the unused parameter.
- All WCT component instance names that are not boundary nodes must include
  the APA or scope identifier to prevent collisions when multiple instances
  load the same config.
- The `app_name` TLA drives the `Pgrapher` instance name and must match
  what `Executor()` registers via `add_app()`.

### Step 5 — Write the PHLEX module

Create `modules/frame_to_deposet.cpp`:

```cpp
#include "wire_cell_phlex/Executor.hpp"
// ... phlex includes ...

PHLEX_FRAMEWORK_REGISTER_MODULE(wcph_frame_to_deposet)
{
    // Declare inputs/outputs:
    auto frame   = input<wcphlex::Frame>(config.get<std::string>("input_layer"),
                                         config.get<std::string>("input_from"));
    auto deposet = output<wcphlex::DepoSet>(config.get<std::string>("output_layer"));

    // Build executor config from PHLEX config:
    boost::json::object ecfg = to_executor_config(config);

    // Construct executor once:
    wcphlex::FrameToDepoSet executor(ecfg);

    execute([=, &executor](auto& event) {
        auto const& f = event.get(frame);
        event.put(deposet, executor(f));
    });
}
```

Register it in `modules/CMakeLists.txt` as a shared library target.

### Step 6 — Verify

```sh
cmake --build build
ctest --test-dir build
```

All existing tests should still pass.  Write a new integration test in
`test/` that exercises the new module end-to-end.

---

## Config keys reference

All keys are read from the `boost::json::object` passed to `Executor()`.
PHLEX modules build this object via `to_executor_config(phlex::configuration)`.

| Key | Type | Required | Description |
|---|---|---|---|
| `wct_config` | string | yes | Path to Jsonnet config file |
| `wct_plugins` | array of strings | no | Additional WCT plugin libraries |
| `wct_app` | string | no (default `"Pgrapher"`) | WCT IApplication type |
| `wct_tla` | object (string→string) | no | Extra Jsonnet TLA assignments |
| `wct_log_sink` | string | no (default `""`) | Route WCT log to this destination: `"stdout"`, `"stderr"`, or a file path |
| `wct_log_level` | string | no (default `""`) | Set WCT log level: `"warn"`, `"info"`, `"debug"`, etc. |
| `module_label` | string | no (injected by PHLEX) | Scope prefix for WCT component names |

The `module_label` key is injected automatically by the PHLEX framework into
every module config.  It becomes `m_scope`, which prefixes all WCT boundary
node names and the Pgrapher instance name.  Two executor instances with
different module labels (`"sim_a"`, `"sim_b"`) create WCT components with
non-colliding names (`sim_a_frame_source`, `sim_b_frame_source`, etc.).
