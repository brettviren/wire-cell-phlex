# wire-cell-phlex Design Options

This document analyzes the major software design decisions for `wire-cell-phlex` and
recommends a path forward for each. It is the output of Phase 1 and guides Phase 2
(implementation).

## Background

`wire-cell-phlex` must bridge two independently-designed data flow frameworks:

- **PHLEX**: DFP via TBB flow graph; data products flow through typed channels routed by
  `(creator, layer, suffix, type_id)`; no global event store; multiple events in flight.
- **WCT**: DFP via Pgrapher/TbbFlow; data products (IData subclasses) flow through typed
  ports; `boost::any` type erasure on edges; nodes implement interface classes;
  pull-based execution with null-pointer EOS sentinel.

The key reference implementation is `larwirecell`, which integrates WCT with the `art`
framework. `art` provides a central event store (`art::Event`) with explicit `get()`/`put()`.
PHLEX does not. This difference has significant architectural consequences.

---

## Decision 1: WCT-PHLEX Execution Model

*How are WCT components invoked from PHLEX? What does a PHLEX node that wraps WCT look like?*

### Option 1A: "Whole-graph executor" — Run a WCT sub-graph per PHLEX transform node

A PHLEX transform node internally manages a complete WCT DFP sub-graph (via
`WireCell::Main`). For each PHLEX event:

1. PHLEX delivers input products (e.g., `IDepoSet::pointer`) to the transform node.
2. The transform node fills WCT boundary-source buffers with this data.
3. The transform node calls `wcmain()` — WCT processes one unit of data and hits EOS.
4. The transform node extracts output from WCT boundary-sink buffers.
5. The transform node returns the output as a PHLEX product.

WCT boundary nodes (sources/sinks at the edges of the WCT sub-graph) implement both a
WCT interface (e.g., `IDepoSetSource`) and a buffer interface that the PHLEX transform
node fills/drains.

```
PHLEX event layer
  │  IDepoSet::pointer (PHLEX product)
  ▼
┌─────────────────────────────────────────┐
│  WctSubgraphExecutor (PHLEX transform)  │
│                                         │
│  BoundarySource (IDepoSetSource) ←─ fill│
│         ↓  WCT DFP graph               │
│  BoundarySource ← BoundarySource        │
│         ↓                              │
│  BoundarySink (IFrameSink) ──→ drain    │
└─────────────────────────────────────────┘
  │  IFrame::pointer (PHLEX product)
  ▼
PHLEX event layer
```

**Advantages:**
- Proven pattern: directly analogous to larwirecell's approach.
- Existing WCT Jsonnet configurations work with minimal changes.
- All of WCT's complex topologies (fan-in, fan-out, multi-port, hydra) are hidden inside
  the executor; PHLEX only sees typed inputs/outputs at the boundary.
- Small number of PHLEX nodes required (one per WCT sub-graph type).
- WCT plugins used as-is; no modification to WCT code.

**Disadvantages:**
- The WCT graph is opaque to PHLEX's scheduler; PHLEX cannot optimize across it.
- PHLEX's TBB parallelism and WCT's internal graph parallelism may conflict or
  under-utilize resources (mitigated by using `concurrency::serial` for the executor).
- The "single-shot" per-event execution relies on WCT graph nodes being designed for
  single-event operation; stateful WCT nodes that accumulate across events are
  incompatible with this model.
- Intermediate WCT products are not directly visible as PHLEX products; only the
  final boundary outputs are.

**Suitability:**  
Best for: running well-established WCT processing pipelines (signal processing,
simulation) where the WCT graph is self-contained and processes one logical event
at a time.

---

### Option 1B: "Node-per-node mapping" — Each WCT node becomes a PHLEX operator

Each WCT node type is wrapped individually as a PHLEX node. The PHLEX graph
replaces WCT's Pgrapher as the graph executor.

| WCT Node Type | PHLEX Equivalent |
|---------------|-----------------|
| `ISourceNode<T>` | provider |
| `IFunctionNode<I,O>` | transform |
| `ISinkNode<T>` | observer / output |
| `IFaninNode<I,O,N>` | fold (approximately) |
| `IFanoutNode<I,O,N>` | ??? |
| `IHydraNode` | ??? |
| `IJoinNode` | transform with multiple input_family |

**Advantages:**
- Full PHLEX scheduling visibility; PHLEX can optimize the combined graph.
- Intermediate WCT products become PHLEX products, observable and debuggable.
- Leverages PHLEX's native concurrency model.

**Disadvantages:**
- WCT node types do not map cleanly to PHLEX categories:
  - WCT fan-out (IFanoutNode) has no PHLEX equivalent.
  - WCT hydra (multi-in, multi-out) has no PHLEX equivalent.
  - WCT's EOS (null pointer) protocol has no PHLEX equivalent.
  - PHLEX's fold is layer-aware; WCT's fanin is not.
- Each WCT node requires a custom PHLEX wrapper with type-erasing adapters.
- `boost::any` ↔ PHLEX typed-product conversion needed at every edge.
- Would require modifying or wrapping every WCT plugin; not maintainable.
- WCT's static compilation of node types fights against PHLEX's function-based model.

**Suitability:**  
Not recommended for the initial implementation due to the incompatibility of node
categories and EOS protocols. Could be revisited if WCT adds PHLEX-aware nodes natively.

---

### Option 1C: "PHLEX-native WCT nodes" — Write new WCT nodes that are also PHLEX operators

New C++ classes that simultaneously satisfy WCT interface requirements AND PHLEX
registration patterns. These would be "dual-mode" nodes at the graph boundary.

Example: A class that is both:
- `WireCell::IDepoSetSource` (a WCT source that WCT pulls from)
- A PHLEX transform (that PHLEX pushes into)

This is the `IArtEventVisitor` pattern from larwirecell, adapted to PHLEX.

**Advantages:**
- Tight coupling between frameworks enables zero-copy data transfer.
- Reuses the converter-component idiom from larwirecell.

**Disadvantages:**
- Requires the same WCT boundary buffers as Option 1A; not fundamentally different.
- These nodes only work at the WCT graph boundary, so Option 1A applies to the interior.

**Suitability:**  
This IS the implementation of the boundary nodes in Option 1A. It is not a separate
option so much as the mechanism used to implement the edges of Option 1A.

---

### **Recommendation: Option 1A ("Whole-graph executor") with Option 1C boundary nodes**

Use the larwirecell architecture adapted to PHLEX:

1. Create `WctBoundarySource<T>` — a class that implements both a WCT source interface
   (e.g., `WireCell::IDepoSetSource`) and has a method to receive PHLEX products.
2. Create `WctBoundarySink<T>` — a class that implements both a WCT sink interface
   (e.g., `WireCell::IFrameSink`) and has a method to deliver results to PHLEX.
3. Create `WctExecutor` — a C++ class that holds `WireCell::Main`, manages
   boundary sources/sinks, and provides a call operator for per-event execution.
4. Wrap `WctExecutor` in PHLEX registration as a transform (or a class with registered
   member functions).

For the initial implementation, provide concrete executor types for the most important
use cases:

| Executor Type | PHLEX Input | PHLEX Output | WCT Use Case |
|---------------|-------------|--------------|--------------|
| `WctDepoSetToFrame` | `IDepoSet::pointer` | `IFrame::pointer` | Simulation (drift+electronics) |
| `WctFrameFilter` | `IFrame::pointer` | `IFrame::pointer` | Signal processing |
| `WctDepoToDepoSet` | `IDepo::pointer` | `IDepoSet::pointer` | Deposition batching |

This answers the question: **we write a small number of specific PHLEX nodes** (one per
boundary-type pair), not a myriad of nodes for every WCT type combination.

---

## Decision 2: WCT IData Types as PHLEX Products

*How are WCT data objects exchanged via PHLEX's product routing system?*

### Background: PHLEX type routing constraints

PHLEX routes products by `(creator, layer, suffix, type_id)`. The `type_id` system
(in `phlex/phlex/model/type_id.hpp`) handles:
- Arithmetic types and their wrappers
- `std::vector<T>` and other contiguous containers (proper type_id)
- **Non-aggregate class types** (including all WCT IData interfaces) → opaque `type_id = 0xFF`

Consequence: `WireCell::IFrame::pointer` and `WireCell::IDepoSet::pointer` (both
`std::shared_ptr<const AbstractType>`) would both receive `type_id = 0xFF`.
PHLEX could not distinguish them by type alone; routing would rely solely on
`(creator, layer, suffix)`. The C++ type-safety is still maintained via dynamic_cast
inside PHLEX's product storage, but two different WCT types with the same suffix
would be ambiguous in the routing specification.

### Option 2A: WCT IData pointer types directly as PHLEX products

Pass `WireCell::IDepo::pointer`, `WireCell::IFrame::pointer`, etc. directly as
PHLEX product types. Use distinct suffixes to compensate for the opaque type_id.

```cpp
// PHLEX provider returning an IDepoSet
m.provide("provide_depos", [](data_cell_index const& id) -> IDepoSet::pointer {
  return /* ... */;
}).output_product({.creator = "sim", .layer = "event", .suffix = "deposits"});

// PHLEX transform accepting IDepoSet, returning IFrame
IFrame::pointer process(IDepoSet::pointer const& depos) {
  /* ... */
}
m.transform("sigproc", process, concurrency::serial)
  .input_family(product_query{.creator = "sim", .layer = "event", .suffix = "deposits"})
  .output_product_suffixes("frames");
```

**Advantages:**
- Zero-copy: WCT shared_ptrs passed through PHLEX without conversion.
- Natural: developers see familiar WCT types in function signatures.
- Minimal boilerplate.

**Disadvantages:**
- Opaque type_id: PHLEX cannot introspect or display the type meaningfully.
- Routing depends entirely on suffixes to distinguish types; a wrong suffix causes
  a silent type mismatch (dynamic_cast returns null, hard to debug).
- WCT is a required header dependency for any PHLEX module handling these products.

---

### Option 2B: Concrete wrapper structs for WCT IData types

Define lightweight concrete C++ structs that hold WCT IData pointers:

```cpp
// In wire-cell-phlex headers:
namespace wcphlex {
  struct Frame    { WireCell::IFrame::pointer    ptr; };
  struct DepoSet  { WireCell::IDepoSet::pointer  ptr; };
  struct Depo     { WireCell::IDepo::pointer     ptr; };
  struct Tensor   { WireCell::ITensor::pointer   ptr; };
}
```

These structs are C++20 aggregates, giving them well-formed type_ids that PHLEX
can distinguish. Routing is correct by type even without distinct suffixes.

```cpp
// PHLEX transform:
wcphlex::Frame process(wcphlex::DepoSet const& depos) {
  auto result = /* ... call WCT ... */;
  return wcphlex::Frame{result};
}
```

**Advantages:**
- Proper type_id: PHLEX routing distinguishes `wcphlex::Frame` from `wcphlex::DepoSet`.
- Type mismatches caught at graph construction time.
- Clean API: `wcphlex::Frame` is more descriptive than `WireCell::IFrame::pointer`.
- Wrappers can be extended with PHLEX-specific metadata if needed.

**Disadvantages:**
- Thin wrapper boilerplate: `wrap.ptr` to access underlying WCT object.
- Requires defining wrappers for all used WCT types.
- Adds one indirection step at every boundary.

---

### Option 2C: Convert to standard C++ types at WCT boundary

Fully convert WCT data types to standard C++ structures at every PHLEX boundary.
E.g., represent an `IFrame` as `std::vector<std::vector<float>>` + metadata struct.

**Advantages:**
- No WCT dependency in downstream PHLEX modules.
- Proper type_ids; PHLEX can introspect types.

**Disadvantages:**
- Data conversion overhead at every boundary: significant for large frames.
- Information loss: WCT's channel mapping, tagging, masking metadata may not
  survive full conversion.
- Downstream code cannot use WCT utilities that expect WCT types.
- Increases development effort significantly.

**Suitability:**  
Appropriate only for output products going to non-WCT consumers (e.g., converting
processed `IFrame` data to `std::vector<float>` for ROOT output). Not suitable as
the primary data exchange mechanism.

---

### **Recommendation: Option 2B (concrete wrapper structs)**

Define a small set of aggregate wrapper structs in `wire-cell-phlex`:

```cpp
namespace wcphlex {
  struct Depo     { WireCell::IDepo::pointer     ptr; };
  struct DepoSet  { WireCell::IDepoSet::pointer  ptr; };
  struct Frame    { WireCell::IFrame::pointer    ptr; };
  struct Tensor   { WireCell::ITensor::pointer   ptr; };
  struct TensorSet{ WireCell::ITensorSet::pointer ptr; };
}
```

This gives proper type_ids for PHLEX routing while keeping WCT data in its native
immutable shared_ptr form. The wrappers are zero-overhead (one extra member access).

For output to non-WCT consumers, use Option 2C conversion selectively: an observer
module converts `wcphlex::Frame` to, say, `std::vector<float>` for ROOT writing.

---

## Decision 3: Configuration Strategy

*How does PHLEX Jsonnet configuration connect with WCT Jsonnet configuration?*

### The problem

PHLEX and WCT both use Jsonnet but with different schemas:

- **PHLEX schema**: A top-level object with `driver`, `sources`, `modules` keys. Module
  config objects contain a `cpp` key (plugin name) and arbitrary per-module config.
- **WCT schema**: A top-level JSON **array** of typed component objects:
  `[{"type":"Pgrapher","name":"","data":{...}}, ...]`. The Pgrapher specifies an
  explicit edge list connecting components by `"type:name"` string.

These schemas are structurally incompatible; one cannot simply be embedded in the other.

### Option 3A: WCT Jsonnet file path(s) as PHLEX config values

The PHLEX module config for a WCT executor contains a path to a WCT Jsonnet file.
PHLEX config manages the PHLEX graph; the WCT Jsonnet file manages the WCT sub-graph.

There are two sub-variants based on how PHLEX values are injected into WCT Jsonnet:

#### Sub-variant 3A-i: External variables (`std.extVar`) — larwirecell pattern

```jsonnet
// PHLEX config
modules: {
  sigproc: {
    cpp: 'wct_frame_filter',
    wct_config: 'cfg/sigproc/uboone.jsonnet',
    wct_plugins: ['WireCellSigProc', 'WireCellGen'],
    wct_vars: { detector: 'uboone', nthreads: '4' },  // passed via add_var()
  }
}
```

```jsonnet
// WCT config (must consume all declared extVars or evaluation fails)
local detector = std.extVar('detector');
local nthreads  = std.parseInt(std.extVar('nthreads'));
[{ type: "Pgrapher", ... }]
```

**Problem**: `std.extVar()` provides no default-value mechanism. Every variable
consumed by the WCT Jsonnet file must be supplied by the caller or the Jsonnet
evaluator will throw an error. This couples the PHLEX config tightly to the WCT
config's internal variable list, making the WCT config non-self-contained. The file
cannot be evaluated standalone (e.g., for testing with `jsonnet file.jsonnet`) without
providing all extVars on the command line.

#### Sub-variant 3A-ii: Top Level Arguments (TLA) — recommended

Jsonnet supports an alternative injection mechanism: when the top-level value of a
Jsonnet file is a **function**, callers invoke it with named arguments. Crucially, the
function can declare **default values** for its parameters.

```jsonnet
// WCT config written as a top-level function (cfg/sigproc/main.jsonnet)
function(
  detector  = 'uboone',   // default: uboone geometry
  nthreads  = 1,          // default: single-threaded
  tag       = 'gauss',    // default: gauss filter tag
)
{
  local wc = import "wirecell.jsonnet";
  local pg = import "pgraph.jsonnet";
  // ... WCT graph config using detector, nthreads, tag
  [
    { type: "Pgrapher", data: { edges: [...] } },
    { type: "OmnibusSigProc", name: "sig", data: { nthreads: nthreads } },
  ]
}
```

```jsonnet
// PHLEX config — only override what differs from WCT defaults
{
  modules: {
    sigproc: {
      cpp: 'wct_frame_filter',
      wct_config: 'cfg/sigproc/main.jsonnet',
      wct_plugins: ['WireCellSigProc', 'WireCellGen'],
      wct_tla: {           // top-level arguments; any not listed use WCT defaults
        detector: 'dune',  // override detector
        nthreads: 4,       // override thread count
        // tag not specified → WCT default 'gauss' is used
      },
    }
  }
}
```

`WireCell::Main` already provides `tla_var(name, value)` and `tla_code(name, expr)`
for exactly this purpose (see `apps/inc/WireCellApps/Main.h` lines 63–68).

**C++ implementation in the WctExecutor:**

```cpp
// In WctExecutor constructor, after reading PHLEX config:
if (auto tlas = config.get_if_present<phlex::configuration>("wct_tla")) {
  for (auto const& [key, val] : *tlas) {
    // Serialize the Boost.JSON value to its JSON representation.
    // JSON is a valid subset of Jsonnet, so tla_code handles all value types
    // (strings, numbers, booleans, objects, arrays) uniformly and correctly.
    m_wcmain.tla_code(key, boost::json::serialize(val));
  }
}
m_wcmain.add_config(wct_config_path);
m_wcmain.initialize();
```

Note: using `tla_code` (rather than `tla_var`) for all values is correct and simpler
than distinguishing by type. JSON is a strict subset of Jsonnet: `tla_code("n", "4")`
passes the integer `4`, while `tla_var("n", "4")` would pass the string `"4"`. Using
`tla_code` with a JSON-serialized value gives the right Jsonnet type automatically.

**Advantages of TLA over extVar:**
- Default values in the WCT Jsonnet function signature — callers need not provide
  every parameter; unspecified ones use their defaults.
- Self-documenting: the function signature lists all accepted parameters and their
  defaults in one place.
- Self-contained: the WCT Jsonnet file can be evaluated standalone (`jsonnet file.jsonnet`)
  with all defaults applied — no extVars required on the command line. This aids
  testing and debugging of WCT configs independently of PHLEX.
- Loose coupling: adding a new parameter with a default to the WCT Jsonnet function
  does not require any change to the PHLEX config.

**Disadvantages:**
- WCT Jsonnet files must be written (or refactored) as top-level functions.
- Existing WCT configs that use `std.extVar()` cannot directly use this mechanism
  without refactoring (though they can be wrapped).

**Compatibility note for existing WCT configs**: Existing `std.extVar()`-based WCT
configs can be adapted by wrapping them in a function:
```jsonnet
// Thin wrapper that converts TLA args to extVars for legacy config
function(detector='uboone', nthreads=1) (
  local std_override = { extVar(k):: if k == 'detector' then detector
                                     else if k == 'nthreads' then std.toString(nthreads)
                                     else super.extVar(k) };
  // ... or simply re-export the values the legacy file needs
)
```
In practice, new WCT configs written for wire-cell-phlex should use the TLA pattern
from the start.

**Advantages (of Option 3A overall):**
- Configuration concerns are separated: PHLEX controls graph structure; WCT controls
  signal-processing details.
- WCT config benefits from the full WCT Jsonnet ecosystem (`cfg/pgraph.jsonnet`, etc.).
- WCT configs can be tested standalone.
- Incremental adoption: each WCT sub-graph has its own config file.

**Disadvantages:**
- Two files to maintain for each WCT sub-graph.
- PHLEX cannot inspect or validate the WCT sub-graph configuration.
- WCT config file must be found at runtime (path management needed).

---

### Option 3B: Inline WCT config as Jsonnet value in PHLEX config

Embed the WCT configuration inline within the PHLEX config using Jsonnet's `import`
or by constructing the WCT config array as a Jsonnet value.

```jsonnet
// Single combined config
local wct_edges = [...];
{
  modules: {
    sigproc: {
      cpp: 'wct_frame_filter',
      wct_config: [  // WCT config as inline array
        { type: "Pgrapher", data: { edges: wct_edges } },
        { type: "OmnibusSigProc", name: "sig", data: { ... } },
      ],
    }
  }
}
```

In C++, deserialize the embedded array as a JSON string and pass to `WireCell::Main`.

**Advantages:**
- Single config file; easier to understand graph as a whole.
- Jsonnet can cross-reference PHLEX and WCT config values.

**Disadvantages:**
- PHLEX's config accessor (`config.get<configuration>("wct_config")`) returns a
  `phlex::configuration` (Boost.JSON object). Converting a JSON array to the format
  expected by `WireCell::Main::add_config()` (which expects a filename to load, not
  an inline value) requires additional plumbing: serialize to a temp file or use
  `WireCell::Persist::loads()` (if available).
- The WCT Jsonnet library functions (import `wirecell.jsonnet`, `pgraph.jsonnet`)
  cannot be used unless PHLEX's Jsonnet evaluator is also given the WCT cfg/ search
  path. PHLEX evaluates its config with its own Jsonnet settings.
- Makes the PHLEX config harder to read; WCT arrays are verbose.

---

### Option 3C: Programmatic WCT graph construction from PHLEX config

Skip WCT Jsonnet entirely. Use the PHLEX config values to programmatically construct
WCT components and connect them in C++ code.

```jsonnet
// PHLEX config only
{
  modules: {
    sigproc: {
      cpp: 'wct_frame_filter',
      wct_plugins: ['WireCellSigProc'],
      wct_components: [
        { type: 'OmnibusSigProc', name: 'sig', nthreads: 4 },
        { type: 'FrameFilter', name: 'ff' },
      ],
      wct_edges: [
        { tail: 'FrameSource', head: 'OmnibusSigProc:sig' },
        { tail: 'OmnibusSigProc:sig', head: 'FrameSink' },
      ],
    }
  }
}
```

**Advantages:**
- Single config system; no WCT Jsonnet files needed.

**Disadvantages:**
- Re-implements WCT's configuration system in PHLEX config.
- Cannot use WCT's Jsonnet library (`pgraph.jsonnet` etc.) which provides powerful
  graph-construction utilities.
- Every WCT component parameter must be re-exposed in the PHLEX schema.
- High development effort with limited benefit.

**Suitability:**  
Not recommended.

---

### **Recommendation: Option 3A-ii (WCT Jsonnet file path with TLA injection)**

Use WCT Jsonnet files written as top-level functions with default arguments, invoked
via `WireCell::Main::tla_code()`. This improves on larwirecell's `extVar` approach by
making WCT configs self-contained and self-documenting:

1. The PHLEX module config for a WCT executor specifies:
   - `wct_config`: path to the WCT Jsonnet file (searched via configured paths)
   - `wct_plugins`: list of WCT plugin library names to load
   - `wct_tla`: object whose key-value pairs are injected as Jsonnet TLA via
     `WireCell::Main::tla_code()` (optional; unspecified keys use WCT defaults)

2. The WCT Jsonnet file is a **function** with defaults:
   ```jsonnet
   function(detector='uboone', nthreads=1) { ... }
   ```

3. `wire-cell-phlex` resolves the config file path and passes it to
   `WireCell::Main::add_config()` after registering TLAs via `tla_code()`.

4. Values in `wct_tla` are serialized from PHLEX's Boost.JSON representation to
   their JSON string equivalent and passed via `tla_code()`, preserving their
   Jsonnet type (integer, boolean, string, object, array) correctly.

For backward compatibility with existing `std.extVar()`-based WCT configs, a
`wct_vars` key (using `add_var()`) can be supported alongside `wct_tla`.

---

## Decision 4: Per-Event vs. Per-Job WCT Initialization

*Should the WCT graph be initialized once per job or once per PHLEX event?*

### Option 4A: Initialize WCT once per job (recommended)

`WireCell::Main::initialize()` is called once in the PHLEX algorithm's setup phase
(in the registration lambda or class constructor). The `operator()()` is called once
per PHLEX event. WCT graph nodes must be "resettable" between calls (stateless
processing nodes are fine; stateful accumulator nodes are not).

This matches the larwirecell pattern (WCLS tool initializes once, calls `process()`
per event).

**Constraints:**
- WCT graph nodes used in this mode must not accumulate state across events.
- Multi-event WCT operations (e.g., noise model estimation across many frames) are
  not directly supported; they would need to be decoupled from the per-event executor.

---

### Option 4B: Re-initialize WCT for each PHLEX event

Call `Main::initialize()` and `Main::finalize()` for every PHLEX event. This allows
stateful WCT nodes but at the cost of per-event initialization overhead (loading
plugins, instantiating components, reading configuration).

**Not recommended** for performance reasons.

---

### Option 4C: WCT handles batches; PHLEX folds over events

A PHLEX fold node accumulates N events' worth of input products, then calls WCT once
with the batch. WCT processes the batch and the PHLEX fold emits the output batch.

This inverts the control: WCT processes multiple events, PHLEX provides batching.
Useful for algorithms that are more efficient on batches (e.g., GPU-based processing).

**Suitability:**  
Optional advanced feature; not needed for initial implementation.

### **Recommendation: Option 4A** (initialize once, call per event)

---

## Decision 5: Scope of Initial Implementation

*What should the first working version of wire-cell-phlex demonstrate?*

Given the design recommendations above, the first implementation milestone should
demonstrate end-to-end functionality with a minimal but complete example:

### Minimal viable example: Frame filter

A PHLEX workflow that:
1. **Provider**: Reads or generates `WireCell::IFrame` objects at the event layer.
2. **Transform**: Runs a WCT frame-processing sub-graph (even a trivial pass-through)
   that takes `wcphlex::Frame` and returns `wcphlex::Frame`.
3. **Observer**: Verifies or outputs the processed frame.

This exercises:
- The `WctBoundarySource<IFrame>` / `WctBoundarySink<IFrame>` buffer pattern
- The `wcphlex::Frame` wrapper type
- The `WctExecutor` class managing `WireCell::Main`
- PHLEX registration (provider + transform + observer)
- Jsonnet config (PHLEX config referencing a WCT config file)
- CMake build (SHARED algorithm library + MODULE registration library)

### Extension: IDepoSet-to-IFrame (simulation)

A PHLEX workflow:
1. **Provider**: Generates `WireCell::IDepoSet` at event layer.
2. **Transform**: WCT drift + electronics simulation sub-graph.
3. **Observer**: Counts traces in the resulting `IFrame`.

---

## Summary of Recommendations

| Decision | Recommendation | Rationale |
|----------|---------------|-----------|
| **Execution model** | Option 1A: Whole-graph executor | Proven (larwirecell), handles complex WCT topologies, small number of PHLEX nodes |
| **Data types** | Option 2B: Concrete wrapper structs | Proper type_ids for PHLEX routing, zero-copy, clean API |
| **Configuration** | Option 3A-ii: WCT config file path + TLA injection | Self-contained WCT configs with defaults, loose coupling, standalone testable |
| **Initialization** | Option 4A: Once per job | Performance, matches larwirecell |
| **Initial scope** | Frame filter example | Exercises all integration points, minimal complexity |

---

## Open Questions

1. **Thread safety of WCT graph re-entry**: **Resolved — all WCT executor transforms
   use `concurrency::serial`.** This prevents multiple PHLEX events from calling the
   same `WireCell::Main` simultaneously. `Pgrapher` is the default WCT app and the
   safe choice. `TbbFlow` is also viable: both share the same TBB thread pool
   (cooperative sharing, not oversubscription), `wait_for_all()` releases the
   blocked PHLEX thread's capacity to TbbFlow tasks, and no circular TBB dependency
   exists. `TbbFlow` is preferred when WCT-internal parallelism (e.g., independent
   per-plane chains) is worth exploiting. Multiple independent WCT executor instances
   can still run in parallel at the PHLEX level since each has its own `WireCell::Main`
   and `concurrency::serial` serializes access per instance.

2. **WCT EOS propagation and graph reset**: After `wcmain()` processes one event, EOS
   has propagated through the graph. Does calling `wcmain()` again on the same
   `WireCell::Main` instance work correctly? The larwirecell pattern suggests yes —
   WCT source boundary nodes immediately re-signal EOS if their buffer is empty,
   causing the graph to do nothing; they buffer data first, then the graph processes
   it. This must be verified empirically.

3. **Multi-output WCT graphs**: Some WCT graphs produce multiple output types
   simultaneously (e.g., both `IFrame` and `IDepoSet`). How to return multiple products
   from one PHLEX transform? Answer: use a tuple return type. E.g.:
   ```cpp
   std::tuple<wcphlex::Frame, wcphlex::DepoSet>
   process(wcphlex::DepoSet const& input);
   ```
   with `.output_product_suffixes("frames", "deposits")`.

4. **WCT geometry services**: WCT nodes often require geometry (wire positions,
   channel maps). In larwirecell, these are provided by art services. In PHLEX, they
   should be job-layer providers. The WCT `IWireSchema`, `IAnodePlane`, etc. objects
   should be loaded by a PHLEX job-layer provider and passed to the WCT executor as
   configuration (via `extVar` referencing a wire geometry file path).

5. **IConfigurable and WCT component state**: WCT's `IConfigurable::configure()` is
   called once during `Main::initialize()`. The executor class holds all WCT state.
   PHLEX's `concurrency::serial` constraint ensures single-threaded access to this state.

6. **Package naming and CMake targets**: Following phlex-examples:
   - `libwire_cell_phlex` (SHARED): WCT boundary adapters, executor class, wrapper types
   - `wct_frame_filter` (MODULE): PHLEX registration for specific executor types
   - `wct_depo_to_frame` (MODULE): PHLEX registration for simulation executor
   - etc.
