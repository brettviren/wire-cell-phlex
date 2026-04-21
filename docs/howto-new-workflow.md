# How to write a new wire-cell-phlex workflow

This guide walks through adding a new WCT sub-graph as a PHLEX workflow, from WCT
Jsonnet configuration through to a passing integration test.  It assumes familiarity
with both Wire-Cell Toolkit configuration and basic PHLEX workflow JSON.

---

## Concepts

### The boundary-node pattern

`wire-cell-phlex` bridges PHLEX's event loop to a WCT sub-graph by inserting
"boundary" nodes at each edge of the WCT graph:

```
PHLEX side                          WCT sub-graph
─────────────────────────────────────────────────────────────────
wcphlex::Frame ─► fill()  ─► FrameBoundarySource
                               │
                               ▼ ... WCT nodes ...
                               │
                         FrameBoundarySink ─► drain() ─► wcphlex::Frame
```

For each event, the PHLEX MODULE:
1. Calls `source.fill(ptr)` to enqueue the input for this event.
2. Calls `wcmain()` to run the WCT Pgrapher until quiescent.
3. Calls `sink.drain()` to collect the output.

The WCT graph runs entirely within a `concurrency::serial` PHLEX node; no TBB
re-entrancy or thread-safety burden falls on the WCT nodes.

### Executor classes

`wcphlex::FrameFilter` and `wcphlex::DepoSetToFrame` are the two concrete executor
classes.  Each owns a `WireCell::Main` instance and wraps it as a callable:

| Executor | Input PHLEX type | Output PHLEX type | Use case |
|----------|------------------|-------------------|-|
| `FrameFilter` | `wcphlex::Frame` | `wcphlex::Frame` | signal processing, noise filtering, … |
| `DepoSetToFrame` | `wcphlex::DepoSet` | `wcphlex::Frame` | drift + electronics simulation |

WCT is initialized **on the first event** (deferred initialization), not at
construction time.  This lets job-layer geometry products (see
[Geometry-aware workflows](#geometry-aware-workflows)) be registered in the
`FacadeWireSchema` static map before WCT's configure phase runs.

---

## Step 1: Write the WCT Jsonnet config

The WCT config is a Jsonnet **function** that accepts named top-level arguments
(TLAs) injected by the executor.  The executor always injects these TLAs:

| TLA | Value set by executor | Purpose |
|-----|-----------------------|---------|
| `source_name` | `<module_label>_frame_source` | FrameBoundarySource instance name |
| `sink_name` | `<module_label>_frame_sink` | FrameBoundarySink / DepoSetBoundarySink name |
| `app_name` | `<module_label>_pgrapher` | Pgrapher instance name |

`module_label` is the key of the module in the PHLEX workflow config (e.g.
`sigproc`), injected by PHLEX.  Using it as a name prefix ensures two instances of
the same module library use distinct WCT factory entries.

**For `FrameFilter`**, start from `cfg/frame-passthrough.jsonnet` and add WCT
processing nodes between the boundary source and sink:

```jsonnet
// cfg/my-signal-processing.jsonnet
function(
    source_name = "wcphlex_frame_source",
    sink_name   = "wcphlex_frame_sink",
    app_name    = "wcphlex_pgrapher",
)
[
    { type: "FrameBoundarySource", name: source_name, data: {} },

    // Add your WCT processing nodes here:
    { type: "MyFilter", name: "myfilter", data: { /* ... */ } },

    { type: "FrameBoundarySink", name: sink_name, data: {} },

    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                { tail: { node: "FrameBoundarySource:" + source_name, port: 0 },
                  head: { node: "MyFilter:myfilter",                  port: 0 } },
                { tail: { node: "MyFilter:myfilter",                  port: 0 },
                  head: { node: "FrameBoundarySink:" + sink_name,     port: 0 } },
            ],
        },
    },
]
```

Place the file under `cfg/` so it is found via `WIRECELL_PATH`.

**For `DepoSetToFrame`**, the boundary source type changes to
`DepoSetBoundarySource`; the sink is still `FrameBoundarySink`.

---

## Step 2: Write the PHLEX workflow config

A minimal workflow uses `wcp_frame_source` to produce synthetic frames, your new
`wcp_frame_filter` instance to process them, and `wcp_frame_observer` to validate:

```jsonnet
// test/my-workflow.jsonnet
{
    driver: {
        cpp: 'generate_layers',
        layers: {
            event: { parent: 'job', total: 5, starting_number: 1 },
        },
    },
    sources: {
        frame_source: {
            cpp: 'wcp_frame_source',
            output_layer: 'event',
        },
    },
    modules: {
        sigproc: {
            cpp: 'wcp_frame_filter',
            wct_config: 'my-signal-processing.jsonnet',
            wct_plugins: ['WireCellPgraph', 'WireCellSigProc'],  // add needed plugins
            input_layer: 'event',
        },
        verify: {
            cpp: 'wcp_frame_observer',
            input_layer: 'event',
            input_from: 'sigproc',
        },
    },
}
```

Key points:
- The `cpp` value is the MODULE library name without the `lib` prefix and `.so`
  suffix (e.g. `wcp_frame_filter` → `libwcp_frame_filter.so`).
- `wct_plugins` lists WCT plugin libraries needed by the sub-graph.  Always include
  `WireCellPgraph` when using Pgrapher.
- `input_layer` / `output_layer` name the PHLEX data layer (not a WCT concept).

---

## Step 3: Add a CTest entry

In the top-level `CMakeLists.txt`, after the existing tests:

```cmake
add_test(NAME phlex_my_workflow
    COMMAND "${PHLEX_EXECUTABLE}" -c
        "${CMAKE_CURRENT_SOURCE_DIR}/test/my-workflow.jsonnet"
)
set_tests_properties(phlex_my_workflow PROPERTIES ENVIRONMENT "${_phlex_env}")
```

Use `_phlex_env_with_data` instead of `_phlex_env` if the workflow needs WCT shared
data files (wire geometry, field response, etc.) from the Spack view.

---

## Multiple instances of the same module

PHLEX supports loading the same `cpp` library under two different module keys.  Each
instance gets an independent product namespace and an independent WCT graph.  The
`module_label` injected by PHLEX ensures the WCT component instance names are
distinct (prefix `sigproc_a_` vs `sigproc_b_`).

```jsonnet
modules: {
    sigproc_a: {
        cpp: 'wcp_frame_filter',
        wct_config: 'my-signal-processing.jsonnet',
        input_layer: 'event',
        input_suffix: 'frame_a',   // reads the "frame_a" product
    },
    sigproc_b: {
        cpp: 'wcp_frame_filter',
        wct_config: 'my-signal-processing.jsonnet',
        input_layer: 'event',
        input_suffix: 'frame_b',   // reads the "frame_b" product
    },
},
```

If the WCT graph also needs distinct node names beyond the boundary nodes (e.g.
two `AnodePlane` instances for different detector modules), pass them via `wct_tla`:

```jsonnet
sigproc_a: {
    wct_tla: { detector_variant: 'module_a' },
    // ...
},
```

---

## Geometry-aware workflows

Many WCT processing nodes (e.g. `AnodePlane`, `ChannelNoiseDB`) require an
`IWireSchema` service during their `configure()` phase.  In `wire-cell-phlex`,
geometry arrives as a **job-layer PHLEX product** (`wcphlex::WireSchema`) and is
bridged into WCT's configure-time service pattern via `FacadeWireSchema`.

### How it works

1. `wcp_wire_schema_source` (job-layer provider) loads a wire geometry file once per
   job using `WireCell::WireSchema::load()`, which searches `WIRECELL_PATH`.  It
   produces a `wcphlex::WireSchema` product at the job layer.

2. `wcp_frame_filter` with `use_wire_schema: true` consumes both the job-layer
   `wcphlex::WireSchema` and an event-layer `wcphlex::Frame`.  On the first event it:
   - Calls `FacadeWireSchema::register_store(scope, ws.store)` to deposit the store
     in a static map keyed by the module's scope (= module_label).
   - Calls `WireCell::Main::initialize()` — during configure, `FacadeWireSchema`
     reads the store from the static map.

3. The WCT Jsonnet config includes a `FacadeWireSchema` component whose `scope` key
   matches the module label, and a `WireSchemaValidator` that confirms the service
   is reachable.  See `cfg/frame-passthrough-with-facade.jsonnet` for a template.

### Workflow config

```jsonnet
{
    driver: { cpp: 'generate_layers',
              layers: { event: { parent: 'job', total: 5 } } },
    sources: {
        geo: {
            cpp: 'wcp_wire_schema_source',
            output_layer: 'job',
            wire_schema_file: 'microboone-celltree-wires-v2.1.json.bz2',
        },
        frame_source: { cpp: 'wcp_frame_source', output_layer: 'event' },
    },
    modules: {
        sigproc: {
            cpp: 'wcp_frame_filter',
            wct_config: 'my-config-with-facade.jsonnet',
            wct_plugins: ['WireCellPgraph'],
            input_layer: 'event',
            use_wire_schema: true,       // consume job-layer WireSchema
        },
        verify: {
            cpp: 'wcp_frame_observer',
            input_layer: 'event',
            input_from: 'sigproc',
        },
    },
}
```

### WCT Jsonnet config template

```jsonnet
// cfg/my-config-with-facade.jsonnet
function(
    source_name      = "wcphlex_frame_source",
    sink_name        = "wcphlex_frame_sink",
    app_name         = "wcphlex_pgrapher",
    wire_schema_name = "wcphlex",         // injected as module_label by executor
)
[
    // Geometry bridge: reads from static registry populated before initialize()
    { type: "FacadeWireSchema", name: wire_schema_name,
      data: { scope: wire_schema_name } },

    // Validation: looks up the FacadeWireSchema service and asserts wires exist
    { type: "WireSchemaValidator", name: wire_schema_name,
      data: { wire_schema: "FacadeWireSchema:" + wire_schema_name } },

    // Your WCT nodes that need IWireSchema:
    { type: "AnodePlane", name: "anode",
      data: { wire_schema: "FacadeWireSchema:" + wire_schema_name, /* ... */ } },

    { type: "FrameBoundarySource", name: source_name, data: {} },
    { type: "FrameBoundarySink",   name: sink_name,   data: {} },
    {
        type: "Pgrapher", name: app_name,
        data: { edges: [ /* ... */ ] },
    },
]
```

> **Note:** `wire_schema_name` is automatically injected by the executor as the
> module label (e.g. `sigproc`) when `use_wire_schema: true`.  It is the key that
> links the static registry entry to the `FacadeWireSchema` WCT component.

---

## Passing extra Jsonnet TLAs

Any arbitrary string→string values can be forwarded from the PHLEX workflow config
to the WCT Jsonnet via `wct_tla`:

```jsonnet
sigproc: {
    cpp: 'wcp_frame_filter',
    wct_config: 'my-config.jsonnet',
    wct_tla: {
        detector: 'uboone',
        n_channels: '8256',
    },
},
```

In the Jsonnet function, declare these as additional parameters:

```jsonnet
function(
    source_name = "...",
    sink_name   = "...",
    app_name    = "...",
    detector    = "uboone",
    n_channels  = 8256,
)
```

---

## Troubleshooting

**`WireCell::ValueError: failed to get node "FrameBoundarySource:..."`**
The Pgrapher tried to wire a node that does not appear in the component list.
Check that the boundary source/sink `name` fields match the TLA values exactly.

**`FacadeWireSchema::configure: no store registered for scope "..."`**
`use_wire_schema: true` is set in the PHLEX workflow but `initialize()` was called
before `register_store()`.  This should not happen with normal usage; check that the
module label matches what the executor injects as `wire_schema_name`.

**`PHLEX_PLUGIN_PATH` errors**
The `phlex` binary searches `PHLEX_PLUGIN_PATH` for MODULE libraries.  Set it to
include the build directory:
```sh
export PHLEX_PLUGIN_PATH=/path/to/build:${PHLEX_PLUGIN_PATH}
```

**`WIRECELL_PATH` errors when loading geometry files**
`WireCell::WireSchema::load()` uses `WireCell::Persist::resolve()`, which searches
`WIRECELL_PATH`.  Set it to include both the `cfg/` directory (for Jsonnet configs)
and the WCT shared data directory (for `.json.bz2` geometry files):
```sh
export WIRECELL_PATH=/path/to/build/../cfg:/path/to/spack/share/wirecell
```
