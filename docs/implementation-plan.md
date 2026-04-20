# wire-cell-phlex Implementation Plan

This document is the output of Phase 2 planning. It breaks down the implementation
into sequential steps, each leaving the package in a state that compiles, links, and
can be exercised by a human before proceeding.

## Confirmed design decisions

Before the steps, two points confirmed after the design-options document:

- **Always use `concurrency::serial`** for any PHLEX node that calls into WCT.
  TbbFlow uses TBB internally; nesting it inside PHLEX's own TBB graph causes thread
  oversubscription and scheduler conflicts. Pgrapher (simple topological-sort execution,
  no TBB) is the correct WCT app to use from within wire-cell-phlex.

- **Multiple instances** of the same `cpp` library under different `modules:` keys
  is the standard mechanism. PHLEX injects the config key as `module_label`, which
  becomes the `plugin` component of every product specification produced by that
  instance. This is the PHLEX equivalent of WCT's `type:name` instance identity.

---

## Package layout (target)

```
wire-cell-phlex/
├── CMakeLists.txt
├── cmake/
│   └── FindWireCell.cmake       # locate WCT libraries from CMake
├── wire_cell_phlex/             # shared library headers + sources
│   ├── Data.h                   # wcphlex wrapper types (Frame, DepoSet, …)
│   ├── BoundarySource.h         # WCT source buffer template
│   ├── BoundarySink.h           # WCT sink buffer template
│   └── Executor.h / Executor.cpp  # WctExecutor: wraps WireCell::Main
├── frame_filter.cpp             # PHLEX MODULE: wcphlex::Frame → wcphlex::Frame
├── depo_to_frame.cpp            # PHLEX MODULE: wcphlex::DepoSet → wcphlex::Frame
├── frame_source.cpp             # PHLEX MODULE: provider of synthetic frames
├── cfg/
│   ├── frame-passthrough.jsonnet  # WCT config: trivial Frame pass-through
│   └── depo-to-frame.jsonnet      # WCT config: drift + electronics simulation
├── test/
│   ├── CMakeLists.txt
│   ├── test_data_types.cpp         # unit test: wrapper types have correct type_ids
│   ├── test_boundary.cpp           # unit test: boundary source/sink without PHLEX
│   ├── test_executor.cpp           # unit test: WctExecutor with trivial WCT graph
│   ├── frame-filter-workflow.jsonnet   # PHLEX integration test workflow
│   └── depo-to-frame-workflow.jsonnet  # PHLEX integration test workflow
└── docs/
    └── *.md
```

---

## Step 1: Package skeleton

**Goal:** A CMake project that locates PHLEX and WCT, compiles a trivial source file,
and runs a do-nothing test. Proves the build environment is correct.

**Files to create:**

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.25)
project(wire_cell_phlex CXX)

set(CMAKE_CXX_STANDARD 23)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(phlex REQUIRED)
find_package(WireCell REQUIRED COMPONENTS Util Iface Aux Apps)
find_package(TBB REQUIRED)

# Shared library: WCT integration code (no PHLEX registration)
add_library(wire_cell_phlex SHARED wire_cell_phlex/Executor.cpp)
target_include_directories(wire_cell_phlex PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(wire_cell_phlex PUBLIC
  WireCell::Apps WireCell::Iface WireCell::Aux WireCell::Util
  phlex::core)

enable_testing()
add_subdirectory(test)
```

`cmake/FindWireCell.cmake`: adapt from `larwirecell/Modules/FindWireCell.cmake`,
which already locates WCT libraries via pkg-config or CMake config files.

`wire_cell_phlex/Executor.cpp`: a stub `#include "wire_cell_phlex/Executor.h"` that
compiles without error.

`test/CMakeLists.txt`: one trivial CTest that runs `/bin/true`.

**Human exercise:**
```sh
cmake -B build -S .
cmake --build build
ctest --test-dir build
```
All should succeed.

---

## Step 2: WCT data wrapper types

**Goal:** Define the `wcphlex` aggregate wrapper structs and verify that PHLEX's type
system gives them distinct, non-opaque `type_id` values, enabling correct product routing.

**Background:** WCT's `IData` interface types are abstract classes. PHLEX assigns
`type_id = 0xFF` (opaque) to abstract types, making them indistinguishable by type in
product routing. Thin aggregate wrapper structs have proper type_ids.

**Files to create:**

`wire_cell_phlex/Data.h`:
```cpp
#pragma once
#include <WireCellIface/IDepo.h>
#include <WireCellIface/IDepoSet.h>
#include <WireCellIface/IFrame.h>
#include <WireCellIface/ITensor.h>
#include <WireCellIface/ITensorSet.h>

namespace wcphlex {
    // Aggregate wrappers: one field, C++20 aggregate = proper type_id in PHLEX
    struct Depo      { WireCell::IDepo::pointer      ptr; };
    struct DepoSet   { WireCell::IDepoSet::pointer   ptr; };
    struct Frame     { WireCell::IFrame::pointer     ptr; };
    struct Tensor    { WireCell::ITensor::pointer    ptr; };
    struct TensorSet { WireCell::ITensorSet::pointer ptr; };
}
```

`test/test_data_types.cpp`: uses PHLEX's `make_type_id<T>()` to assert that
`wcphlex::Frame`, `wcphlex::DepoSet`, `wcphlex::Depo` each have a type_id that is
neither 0xFF (opaque) nor equal to each other. Also exercises construction from a
`nullptr` WCT pointer and move semantics.

Add to `wire_cell_phlex/Executor.cpp` a `#include "wire_cell_phlex/Data.h"` to ensure
the header compiles as part of the shared library.

**Human exercise:**
```sh
cmake --build build && ctest --test-dir build -R data_types
```

---

## Step 3: Boundary source and sink buffer classes

**Goal:** Template classes that implement WCT source/sink interfaces and act as
buffers at the WCT graph boundary. These are the "dual-interface" components
analogous to larwirecell's `IArtEventVisitor` converters.

**Design:** For each WCT interface type, a boundary class:
- Inherits from the WCT source or sink interface
- Holds a small buffer (one event's worth of data)
- A `fill(data)` method lets the PHLEX side pre-load the buffer before `wcmain()`
- A `drain()` method lets the PHLEX side retrieve results after `wcmain()`
- The WCT `operator()` serves from the buffer (source) or accumulates into it (sink)

`wire_cell_phlex/BoundarySource.h`:
```cpp
#pragma once
#include <WireCellIface/IQueuedOutNode.h>
// (or ISourceNode, depending on which interface fits best for batched data)

namespace wcphlex {

// A WCT source that serves a single pre-filled object, then sends EOS.
// T must be a WCT IData pointer type (e.g. IDepoSet::pointer).
template <typename WctSourceIface, typename DataPtr>
class BoundarySource : public WctSourceIface,
                       public WireCell::IConfigurable {
public:
    using output_pointer = DataPtr;

    // Called by PHLEX before wcmain(): load one event's data
    void fill(DataPtr data) {
        m_data = std::move(data);
        m_served = false;
    }

    // WCT source interface: serve once, then EOS
    bool operator()(output_pointer& out) override {
        if (!m_served) {
            out = m_data;
            m_served = true;
        } else {
            out = nullptr;  // EOS
        }
        return true;
    }

    WireCell::Configuration default_configuration() const override { return {}; }
    void configure(const WireCell::Configuration&) override {}

private:
    DataPtr m_data;
    bool m_served{true};  // true = nothing to serve yet
};

}
```

`wire_cell_phlex/BoundarySink.h`:
```cpp
template <typename WctSinkIface, typename DataPtr>
class BoundarySink : public WctSinkIface,
                     public WireCell::IConfigurable {
public:
    using input_pointer = DataPtr;

    // Called by PHLEX after wcmain(): retrieve accumulated result
    DataPtr drain() { return std::exchange(m_data, nullptr); }

    // WCT sink interface: accumulate (for single-item: just store it)
    bool operator()(const input_pointer& in) override {
        if (in) m_data = in;  // nullptr = EOS, ignore
        return true;
    }

    WireCell::Configuration default_configuration() const override { return {}; }
    void configure(const WireCell::Configuration&) override {}

private:
    DataPtr m_data;
};
```

`WIRECELL_FACTORY` registration lines for each concrete boundary type (e.g.,
`BoundarySource<IDepoSetSource, IDepoSet::pointer>`) are added so WCT's factory
can instantiate them by name.

`test/test_boundary.cpp`: creates a `BoundarySource` and `BoundarySink` pair,
calls `fill()`, calls `operator()()` twice (first call serves data, second is EOS),
calls `drain()`, asserts the round-tripped data pointer is unchanged.

**Human exercise:**
```sh
cmake --build build && ctest --test-dir build -R boundary
```

---

## Step 4: WctExecutor class

**Goal:** A C++ class that:
1. Owns a `WireCell::Main` instance
2. Is constructed from PHLEX config (`wct_config`, `wct_plugins`, `wct_tla`)
3. Registers named boundary source/sink nodes with WCT's factory before `initialize()`
4. Provides a `process(input) -> output` call for per-event use

**Note on WCT app choice:** The executor defaults to `Pgrapher` but allows
`TbbFlow` via a `wct_app` TLA. Both are viable inside a `concurrency::serial`
PHLEX executor:

- **Pgrapher** (default): simple topological-sort execution, no TBB, predictable
  for debugging. All WCT nodes run sequentially regardless of their declared
  concurrency.
- **TbbFlow**: uses TBB internally, but shares PHLEX's thread pool rather than
  competing with it. `wait_for_all()` blocks the one PHLEX-assigned thread while
  TbbFlow tasks run on other pooled threads. No deadlock or oversubscription.
  Useful when the WCT graph has internal parallelism worth exploiting (e.g., three
  wire-plane processing chains running concurrently).

Start with Pgrapher for initial development; switch to TbbFlow if WCT-internal
parallelism is needed.

`wire_cell_phlex/Executor.h`:
```cpp
#pragma once
#include "wire_cell_phlex/Data.h"
#include "wire_cell_phlex/BoundarySource.h"
#include "wire_cell_phlex/BoundarySink.h"
#include <WireCellApps/Main.h>
#include <phlex/configuration.hpp>

namespace wcphlex {

class Executor {
public:
    explicit Executor(phlex::configuration const& config);
    ~Executor();

    // Per-event call: fill sources, run WCT, drain sinks.
    // Concrete typed subclasses provide strongly-typed overloads.
    // Returns false if WCT graph signals a permanent error.
    bool run_once();

protected:
    WireCell::Main m_wcmain;

    // Subclasses register their boundary nodes before calling initialize():
    void initialize();

    // Helper: inject a BoundarySource into the WCT factory with a given name,
    // so it can be referenced by the WCT Jsonnet config.
    template <typename Src>
    std::shared_ptr<Src> register_source(const std::string& name);

    template <typename Snk>
    std::shared_ptr<Snk> register_sink(const std::string& name);
};

// Concrete executor: IDepoSet → IFrame (simulation pipeline)
class DepoSetToFrame : public Executor {
public:
    explicit DepoSetToFrame(phlex::configuration const& config);
    Frame operator()(DepoSet const& input);
private:
    std::shared_ptr</* BoundarySource type */> m_source;
    std::shared_ptr</* BoundarySink type  */> m_sink;
};

// Concrete executor: IFrame → IFrame (signal processing)
class FrameFilter : public Executor {
public:
    explicit FrameFilter(phlex::configuration const& config);
    Frame operator()(Frame const& input);
private:
    std::shared_ptr</* ... */> m_source;
    std::shared_ptr</* ... */> m_sink;
};

}
```

`wire_cell_phlex/Executor.cpp`: Implementation. The constructor:
1. Reads `wct_config`, `wct_plugins` from config
2. Reads optional `wct_tla` sub-object; for each key/value calls
   `m_wcmain.tla_code(key, boost::json::serialize(value))`
3. Adds `"Pgrapher"` as the WCT app
4. Subclass constructor registers boundary nodes, then calls `initialize()`

`cfg/frame-passthrough.jsonnet`:
```jsonnet
// Minimal WCT config: boundary source → boundary sink (no processing)
function(
    source_name = "wcphlex_frame_source",
    sink_name   = "wcphlex_frame_sink",
)
[
    { type: "Pgrapher",
      data: { edges: [
          { tail: { node: source_name, port: 0 },
            head: { node: sink_name,   port: 0 } },
      ]}},
    { type: "FrameBoundarySink", name: sink_name,   data: {} },
    { type: "FrameBoundarySource", name: source_name, data: {} },
]
```

`test/test_executor.cpp`:
- Constructs a `FrameFilter` with config pointing at `cfg/frame-passthrough.jsonnet`
- Calls it with a synthetic `wcphlex::Frame` wrapping a minimal `IFrame` object
- Asserts the returned `wcphlex::Frame` is non-null
- Calls it 10 times (verifying the re-run-after-EOS protocol works correctly)

**Human exercise:**
```sh
cmake --build build && ctest --test-dir build -R executor
```
This is the first test that actually runs a WCT graph end-to-end.

---

## Step 5: First PHLEX module — frame filter

**Goal:** A complete, runnable PHLEX workflow. A PHLEX MODULE that wraps a
`FrameFilter` executor, plus a synthetic frame provider and observer, all wired
together in a Jsonnet workflow config.

**Files:**

`frame_filter.cpp`:
```cpp
#include "phlex/module.hpp"
#include "wire_cell_phlex/Executor.h"

PHLEX_REGISTER_ALGORITHMS(m, config) {
  m.make<wcphlex::FrameFilter>(config)
    .transform("filter", &wcphlex::FrameFilter::operator(), concurrency::serial)
    .input_family(
      phlex::product_query{
        .creator = config.get<std::string>("input_from"),
        .layer   = config.get<std::string>("layer"),
        .suffix  = config.get<std::string>("input_suffix", "frames"),
      })
    .output_product_suffixes(
      config.get<std::string>("output_suffix", "frames"));
}
```

`frame_source.cpp`:
```cpp
#include "phlex/source.hpp"
#include "wire_cell_phlex/Data.h"
// ... create a minimal synthetic IFrame per event

PHLEX_REGISTER_PROVIDERS(m, config) {
  auto const layer = config.get<std::string>("layer");
  m.provide("provide_frames",
    [](phlex::data_cell_index const& id) -> wcphlex::Frame {
      // build a minimal synthetic IFrame using WCT's SimpleFrame
      return wcphlex::Frame{ make_synthetic_frame(id.number()) };
    })
    .output_product({.creator = "frame_source", .layer = layer, .suffix = "frames"});
}
```

`test/frame-filter-workflow.jsonnet`:
```jsonnet
{
  driver: {
    cpp: 'generate_layers',
    layers: { event: { parent: 'job', total: 5 } },
  },
  sources: {
    frame_source: {
      cpp: 'frame_source',
      layer: 'event',
    },
  },
  modules: {
    sigproc: {
      cpp: 'frame_filter',
      layer: 'event',
      input_from: 'frame_source',
      wct_config: 'cfg/frame-passthrough.jsonnet',
      wct_plugins: [],   // passthrough needs no extra plugins
    },
    verify: {
      cpp: 'frame_observer',   // simple observer that checks frame is non-null
      layer: 'event',
      input_from: 'sigproc',
    },
  },
}
```

Add a `frame_observer.cpp` MODULE with a PHLEX observe node that asserts the
received `wcphlex::Frame` is non-null and logs the frame ID.

**Human exercise:**
```sh
phlex-app test/frame-filter-workflow.jsonnet
```
(or however the PHLEX application is invoked — check phlex-examples for the exact
invocation). This is the first end-to-end PHLEX+WCT workflow.

---

## Step 6: IDepoSet-to-IFrame executor (simulation use case)

**Goal:** The second concrete executor type, demonstrating a non-trivial WCT graph
that converts depositions to ADC frames — the core WCT simulation pipeline.

**Files:**

`cfg/depo-to-frame.jsonnet`:
```jsonnet
// WCT simulation sub-graph: IDepoSet source → drift → electronics → IFrame sink
function(
    detector    = 'pdsp',
    source_name = 'wcphlex_deposet_source',
    sink_name   = 'wcphlex_frame_sink',
    // ... other tunables with defaults
)
{
    local wc = import "wirecell.jsonnet";
    local pg = import "pgraph.jsonnet";
    // ... WCT Pgrapher edges connecting sim nodes
}
```

`depo_to_frame.cpp`:
```cpp
#include "phlex/module.hpp"
#include "wire_cell_phlex/Executor.h"

PHLEX_REGISTER_ALGORITHMS(m, config) {
  m.make<wcphlex::DepoSetToFrame>(config)
    .transform("simulate", &wcphlex::DepoSetToFrame::operator(), concurrency::serial)
    .input_family(product_query{
      .creator = config.get<std::string>("input_from"),
      .layer   = config.get<std::string>("layer"),
      .suffix  = "deposits"})
    .output_product_suffixes("frames");
}
```

`test/depo-to-frame-workflow.jsonnet`:
Synthetic `IDepoSet` provider → `depo_to_frame` transform → frame observer.

**Human exercise:**
```sh
phlex-app test/depo-to-frame-workflow.jsonnet
```
This is the first test of an actual physics-relevant WCT sub-graph (not passthrough).

---

## Step 7: Multiple module instances

**Goal:** Demonstrate that the same `cpp` library loaded under two different module
keys works correctly, with independent configuration and independent product namespaces.
This validates the "multiple WCT instances" use case.

**Files:**

`test/multi-instance-workflow.jsonnet`:
```jsonnet
// Two independent sigproc instances on two different input streams
{
  driver: { cpp: 'generate_layers', layers: { event: { parent: 'job', total: 5 } } },
  sources: {
    source_a: { cpp: 'frame_source', layer: 'event', variant: 'a' },
    source_b: { cpp: 'frame_source', layer: 'event', variant: 'b' },
  },
  modules: {
    sigproc_a: {
      cpp: 'frame_filter',
      layer: 'event',
      input_from: 'source_a',
      wct_config: 'cfg/frame-passthrough.jsonnet',
    },
    sigproc_b: {
      cpp: 'frame_filter',
      layer: 'event',
      input_from: 'source_b',
      wct_config: 'cfg/frame-passthrough.jsonnet',
    },
    verify: {
      cpp: 'multi_frame_observer',
      layer: 'event',
      // receives both sigproc_a and sigproc_b outputs and checks they're distinct
    },
  },
}
```

This requires the `frame_source` module to accept a `variant` parameter that
embeds the variant name in the output product suffix, and the frame observer to
consume two products from two different creators.

**Human exercise:**
```sh
phlex-app test/multi-instance-workflow.jsonnet
```

---

## Step 8: Job-layer geometry provider

**Goal:** Demonstrate the job-layer / event-layer separation for WCT geometry.
Many WCT processing nodes require a geometry description (wire positions, channel
maps) that is loaded once per job, not once per event.

**Design:** A PHLEX job-layer provider produces a `wcphlex::WireSchema` (or similar
wrapper around `WireCell::IWireSchema::pointer`) from a geometry file. The WCT
executor for signal processing receives the geometry file path via `wct_tla` (baked
into config at PHLEX registration time from the PHLEX job-layer product). This step
exercises the interaction between PHLEX layer hierarchy and WCT configuration.

Note: WCT geometry objects are typically loaded inside WCT via its own configuration
(specifying a wire geometry file). The PHLEX config's `wct_tla` provides the path
to that file. A job-layer PHLEX provider that validates the geometry file exists
and passes its path to the WCT executor is sufficient for this step.

`test/geometry-workflow.jsonnet`:
```jsonnet
{
  driver: { cpp: 'generate_layers', layers: { event: { parent: 'job', total: 5 } } },
  sources: {
    frame_source:  { cpp: 'frame_source', layer: 'event' },
  },
  modules: {
    sigproc: {
      cpp: 'frame_filter',
      layer: 'event',
      input_from: 'frame_source',
      wct_config: 'cfg/frame-with-geometry.jsonnet',
      wct_tla: { geometry_file: 'wires/pdsp-wires.json.bz2' },
    },
    verify: { cpp: 'frame_observer', layer: 'event', input_from: 'sigproc' },
  },
}
```

**Human exercise:**
```sh
phlex-app test/geometry-workflow.jsonnet
```

---

## Step 9: Additional WCT data types

**Goal:** Complete the set of `wcphlex` wrapper types and boundary classes beyond
`Frame` and `DepoSet`. Add `Depo`, `Tensor`, `TensorSet` wrappers with corresponding
boundary source/sink classes, registered with WCT's factory.

These enable future workflows involving:
- `Depo`-level simulation (individual ionization deposits before batching)
- `TensorSet`-based ML inference (WCT's ONNX/Triton integration)

No new PHLEX MODULE is strictly required at this step; the focus is completing
the library layer. Unit tests cover each new type.

---

## Step 10: Documentation and packaging

**Goal:** Package is ready for others to use.

**Files:**
- `README.md`: overview, build instructions, quick-start example
- `docs/howto-new-workflow.md`: step-by-step guide for writing a new
  wire-cell-phlex workflow, from WCT Jsonnet config to PHLEX workflow config
- CMake install rules so the package can be found by downstream packages
- Any remaining `TODO` comments resolved or filed as issues

---

## Dependencies between steps

```
Step 1 (skeleton)
    └── Step 2 (wrapper types)
            └── Step 3 (boundary classes)
                    └── Step 4 (executor class)
                            ├── Step 5 (frame filter module) ──── Step 7 (multi-instance)
                            ├── Step 6 (depo-to-frame module)
                            └── Step 8 (geometry)
                                        └── Step 9 (remaining types)
                                                    └── Step 10 (docs)
```

Steps 5, 6, and 8 are independent of each other once Step 4 is done.

---

## CMake targets summary

| Target | Type | Links | Provides |
|--------|------|-------|---------|
| `wire_cell_phlex` | SHARED | WireCell::*, phlex::core | Data.h, Executor, BoundarySource, BoundarySink |
| `frame_source` | MODULE | wire_cell_phlex, phlex::module | PHLEX provider: synthetic frames |
| `frame_filter` | MODULE | wire_cell_phlex, phlex::module | PHLEX transform: Frame→Frame |
| `depo_to_frame` | MODULE | wire_cell_phlex, phlex::module | PHLEX transform: DepoSet→Frame |
| `frame_observer` | MODULE | wire_cell_phlex, phlex::module | PHLEX observer: frame validation |
| `test_data_types` | executable | wire_cell_phlex | Step 2 unit test |
| `test_boundary` | executable | wire_cell_phlex | Step 3 unit test |
| `test_executor` | executable | wire_cell_phlex | Step 4 unit test |

---

## Notes for each step

### On WCT factory registration of boundary nodes (Steps 3–4)

WCT's factory discovers components via `WIRECELL_FACTORY(Name, Class, Interfaces...)`.
The boundary source and sink classes must be registered this way so that the WCT
Jsonnet config can reference them by name (e.g., `type: "FrameBoundarySource"`).
This registration is placed in `Executor.cpp` (or a dedicated `BoundaryNodes.cpp`).

The boundary node type names appearing in `WIRECELL_FACTORY` must exactly match
those used in WCT Jsonnet config files (e.g., `cfg/frame-passthrough.jsonnet`).

### On the re-run protocol verification (Step 4)

The `test_executor.cpp` must verify that calling the executor 10+ times on the
same instance works correctly — each call processes one synthetic frame and returns
a result. This validates that the WCT graph cleanly processes one event and resets
for the next. Any state remaining in WCT graph nodes after EOS propagation must
not corrupt subsequent calls.

If re-running fails, the executor may need to store and restore node state between
calls, or re-initialize the WCT graph (at per-event cost). This should be discovered
and resolved in Step 4 before proceeding to PHLEX integration in Step 5.

### On `input_from` configuration (Steps 5–7)

The recommended pattern is that each PHLEX MODULE reads `input_from` from its
config to determine which module's products to consume:

```cpp
product_query{
  .creator = config.get<std::string>("input_from"),
  .layer   = config.get<std::string>("layer"),
  .suffix  = "frames",
}
```

This makes the PHLEX graph topology declarative in the Jsonnet config without
requiring C++ changes. It is the PHLEX equivalent of WCT's Pgrapher edge list.

### On Pgrapher vs TbbFlow (all steps)

The executor defaults to `Pgrapher` but exposes a `wct_app` TLA (default
`'Pgrapher'`) allowing the user to select `'TbbFlow'` when desired. The WCT
Jsonnet config must include a component of the matching type.

`TbbFlow` is viable inside a `concurrency::serial` PHLEX executor because both
share the same TBB thread pool — this is cooperative sharing, not oversubscription.
The blocking `wait_for_all()` call releases the PHLEX-assigned thread's capacity
to TbbFlow tasks running on other pooled threads. No circular TBB task dependency
exists because the PHLEX WCT executor is a leaf node in the PHLEX graph.

Use Pgrapher for initial development and simple pipelines. Use TbbFlow when
the WCT sub-graph has genuine internal parallelism (e.g., independent per-plane
processing chains) and the added complexity is justified.
