# larwirecell Summary

This document summarizes the `larwirecell/` package: the integration of Wire-Cell
Toolkit (WCT) with the "art" framework (LArSoft). It is the primary reference for
design patterns to **reuse** and **avoid** when building `wire-cell-phlex`.

## Repository Structure

```
larwirecell/
├── CMakeLists.txt            # Top-level: finds art, WireCell, LArSoft
├── CMakePresets.json
├── Modules/
│   ├── CMakeLists.txt
│   ├── FindWireCell.cmake    # Reference: how to locate WCT from CMake
│   └── Findjsonnet.cmake
└── larwirecell/
    ├── CMakeLists.txt
    ├── Interfaces/
    │   ├── MainTool.h        # art::tool interface for WCT execution
    │   └── IArtEventVisitor.h  # Bridge: WCT component + art::Event access
    ├── Modules/
    │   └── WireCellToolkit_module.cc  # art::SharedProducer entry point
    ├── Tools/
    │   └── WCLS_tool.cc      # MainTool implementation: runs WCT::Main
    └── Components/
        ├── SimDepoSource.h/.cxx
        ├── SimDepoSetSource.h/.cxx
        ├── RawFrameSource.h/.cxx
        ├── LazyFrameSource.h/.cxx
        ├── CookedFrameSource.h/.cxx
        ├── CookedFrameSink.h/.cxx
        ├── SimChannelSink.h/.cxx
        ├── DepoSetSimChannelSink.h/.cxx
        ├── FrameSaver.h/.cxx
        ├── DepoFluxWriter.h/.cxx
        ├── OpFlashSource.h/.cxx
        ├── ChannelNoiseDB.h/.cxx
        └── MultiChannelNoiseDB.h/.cxx
```

## Overall Architecture

larwirecell integrates WCT into art as a **single art module that wraps the entire
WCT graph**. Art sees only one module (`WireCellToolkit`); the WCT graph structure is
completely opaque to art.

```
art framework
    │
    ▼
WireCellToolkit (art::SharedProducer)
    │
    ▼  (delegates to)
WCLS (art::tool / MainTool)
    │
    ├─ Initialize once: WireCell::Main::initialize()
    │
    └─ Per-event:
         ├─ inputers[i].visit(art::Event)   ← transfer art→WCT
         ├─ wcmain()                        ← execute WCT graph
         └─ outputers[i].visit(art::Event)  ← transfer WCT→art
```

The WCT graph is built once from Jsonnet config and reused for all events.

## The art Framework Model (vs. PHLEX)

### art's Central Event Store

```cpp
// art module's produce() method
void produce(art::Event& event, ...) {
  // READ: get data product by label
  auto depos = event.getValidHandle<std::vector<sim::SimEnergyDeposit>>("simmer");
  
  // WRITE: put result back into event store
  event.put(std::make_unique<std::vector<recob::Wire>>(std::move(wires)));
}
```

Art modules explicitly pull inputs and push outputs through a central `art::Event`
object. This is a **synchronous, single-event-at-a-time** model.

### Why This Is Incompatible with PHLEX

| Aspect | art | PHLEX |
|--------|-----|-------|
| Data access | Central `art::Event` store with `get()`/`put()` | Explicit input/output channels (ports) |
| Event parallelism | One event at a time per module | Multiple events in-flight simultaneously |
| Synchrony | Synchronous event loop | Asynchronous DFP graph |
| Module state | Can be shared across an event path | Operators are independent graph nodes |

In PHLEX, there is **no `art::Event` equivalent**. Data arrives via typed input
channels and leaves via typed output channels. There is no global product store.

## Key Interfaces

### IArtEventVisitor

The bridge between WCT components and art:

```cpp
// larwirecell/Interfaces/IArtEventVisitor.h
class IArtEventVisitor : public WireCell::IComponent<IArtEventVisitor> {
public:
  virtual void produces(art::ProducesCollector& collector) {}  // declare outputs (once)
  virtual void visit(art::Event& event) = 0;                   // access event data
};
```

Converter components inherit **both** `IArtEventVisitor` and a WCT interface.
This dual inheritance is the core integration pattern.

### MainTool

```cpp
// larwirecell/Interfaces/MainTool.h
struct MainTool : virtual art::tool {
  virtual void produces(art::ProducesCollector& collector) = 0;
  virtual void process(art::Event& event) = 0;
};
```

### WireCellToolkit Module

```cpp
// larwirecell/Modules/WireCellToolkit_module.cc
class WireCellToolkit : public art::SharedProducer {
  std::unique_ptr<MainTool> m_wcls;
  
  void produce(art::Event& evt, art::ProcessingFrame const&) override {
    m_wcls->process(evt);
  }
};
```

### WCLS Tool (MainTool implementation)

```cpp
// larwirecell/Tools/WCLS_tool.cc
class WCLS : public MainTool {
  WireCell::Main m_wcmain;
  std::vector<IArtEventVisitor::pointer> m_inputers;
  std::vector<IArtEventVisitor::pointer> m_outputers;

  // Constructor: parse FHiCL config, call m_wcmain.add_*(), call m_wcmain.initialize()
  // After initialize(), discover inputers/outputers by name via Factory::find_tn()

  void process(art::Event& event) override {
    for (auto& iaev : m_inputers)  iaev->visit(event);  // art→WCT
    m_wcmain();                                          // execute graph
    for (auto& iaev : m_outputers) iaev->visit(event);  // WCT→art
  }
};
```

## Converter Components

### Source Converters (art → WCT)

Each implements `IArtEventVisitor` + a WCT source interface:

| Component | WCT Interface | art Input Type | WCT Output Type |
|-----------|---------------|----------------|-----------------|
| `SimDepoSource` | `IDepoSource` | `sim::SimEnergyDeposit` | `IDepo` |
| `SimDepoSetSource` | `IDepoSetSource` | `sim::SimEnergyDeposit` | `IDepoSet` |
| `RawFrameSource` | `IFrameSource` | `raw::RawDigit` | `IFrame` |
| `LazyFrameSource` | `IFrameSource` | `raw::RawDigit` | `IFrame` (lazy) |
| `CookedFrameSource` | `IFrameSource` | `recob::Wire` | `IFrame` |
| `OpFlashSource` | `ITensorSetSource` | `recob::OpFlash` | `ITensorSet` |

**Pattern** (SimDepoSource):
```cpp
class SimDepoSource : public IArtEventVisitor,
                      public WireCell::IDepoSource,
                      public WireCell::IConfigurable {
  std::vector<WireCell::IDepo::pointer> m_depos;
  size_t m_cursor{0};

  void visit(art::Event& event) override {
    // Read art data product, convert, store in m_depos
    auto handle = event.getValidHandle<std::vector<sim::SimEnergyDeposit>>(m_inputtag);
    m_depos = convert(*handle);
    m_cursor = 0;
  }

  bool operator()(output_pointer& out) override {
    // Called by WCT graph: serve from m_depos buffer
    if (m_cursor < m_depos.size()) {
      out = m_depos[m_cursor++];
      return true;
    }
    out = nullptr;  // EOS
    return true;
  }
};
```

### Sink Converters (WCT → art)

| Component | WCT Interface | WCT Input Type | art Output Type |
|-----------|---------------|----------------|-----------------|
| `CookedFrameSink` | `IFrameSink` | `IFrame` | `recob::Wire` |
| `SimChannelSink` | `IDepoFilter` | `IDepo` | `sim::SimChannel` |
| `DepoSetSimChannelSink` | `IDepoSetFilter` | `IDepoSet` | `sim::SimChannel` |
| `FrameSaver` | `IFrameFilter` | `IFrame` | `raw::RawDigit` / `recob::Wire` |
| `DepoFluxWriter` | `IDepoSetFilter` | `IDepoSet` | `sim::SimChannel` |

**Pattern** (CookedFrameSink):
```cpp
class CookedFrameSink : public IArtEventVisitor,
                        public WireCell::IFrameSink,
                        public WireCell::IConfigurable {
  std::vector<recob::Wire> m_wires;

  bool operator()(const input_pointer& in) override {
    // Called by WCT graph: accumulate frames
    if (!in) return true;  // EOS, do nothing
    auto converted = convert(*in);
    m_wires.insert(m_wires.end(), converted.begin(), converted.end());
    return true;
  }

  void visit(art::Event& event) override {
    // Called after WCT graph: put accumulated results into art::Event
    event.put(std::make_unique<std::vector<recob::Wire>>(std::move(m_wires)));
    m_wires.clear();
  }
};
```

## Configuration: FHiCL → WCT Jsonnet

### art/FHiCL Configuration

```fhicl
physics.producers.simmer: {
  module_type: WireCellToolkit
  wcls_main: {
    tool_type: WCLS
    apps: ["Pgrapher"]
    plugins: ["WireCellPgraph", "WireCellGen", "WireCellLarsoft"]
    configs: ["pgrapher/uboone/sim.jsonnet"]
    inputers: ["wclsSimDepoSource"]
    outputers: ["wclsFrameSaver"]
    params: {
      detector: "uboone"
      driftSpeed: 0.111  # mm/us
    }
  }
}
```

### Parameter Injection

Each `params` entry becomes a Jsonnet `extVar`:
```cpp
// In WCLS constructor:
for (auto [key, value] : wclscfg.params) {
  m_wcmain.add_var(key, value);  // → std::extVar('key') in Jsonnet
}
```

In Jsonnet config:
```jsonnet
local detector = std.extVar('detector');
local drift_speed = std.parseJson(std.extVar('driftSpeed')) * wc.mm / wc.us;
```

### Component Naming

Converter components are discovered by name after `initialize()`:
```cpp
// In WCLS constructor (after m_wcmain.initialize()):
for (auto inputer_tn : wclscfg.inputers) {
  auto iaev = WireCell::Factory::find_tn<IArtEventVisitor>(inputer_tn);
  m_inputers.push_back(iaev);
}
```

The names in `inputers`/`outputers` must match the WCT component type:name.

## Data Type Conversions

### sim::SimEnergyDeposit → IDepo

```
SimEnergyDeposit {
  midPoint() [cm]     → IDepo::pos() [mm, after unit conversion]
  T0() [ns]          → IDepo::time() [us]
  NumElectrons()     → IDepo::charge()
  Energy() [MeV]     → IDepo::energy()
  TrackID()          → IDepo::id()
  PdgCode()          → IDepo::pdg()
}
```

### raw::RawDigit → IFrame/ITrace

```
RawDigit {
  Channel()          → ITrace::channel()
  ADCs() [int16]     → ITrace::charge() [float, after pedestal subtraction]
  Samples()          → defines trace length
}
Multiple RawDigits → single IFrame with one ITrace per channel
```

### IFrame → recob::Wire

```
IFrame {
  tagged_traces(tag) → per-channel waveform
  ITrace::channel()  → recob::Wire::Channel()
  ITrace::charge()   → recob::Wire::Signal() (ROI)
}
```

## CMake Build Structure

```cmake
# larwirecell/Modules/CMakeLists.txt — finds WCT
find_package(WireCell REQUIRED COMPONENTS Util Iface Aux Apps)
# Uses Modules/FindWireCell.cmake

# Components library
add_library(WireCellLarsoft SHARED
  larwirecell/Components/SimDepoSource.cxx
  larwirecell/Components/CookedFrameSink.cxx
  # ...
)
target_link_libraries(WireCellLarsoft PUBLIC
  WireCell::Util WireCell::Iface WireCell::Aux
  lardataobj::RecoBase larsimobj::Simulation
  art_Framework_Principal art_Framework_Core
)

# Tool
add_library(WCLS_tool MODULE larwirecell/Tools/WCLS_tool.cc)
target_link_libraries(WCLS_tool PRIVATE WireCell::Apps fhiclcpp::types)

# Module
add_library(WireCellToolkit_module MODULE larwirecell/Modules/WireCellToolkit_module.cc)
target_link_libraries(WireCellToolkit_module PRIVATE art_Framework_Core)
```

## Lessons and Recommendations for wire-cell-phlex

### Patterns to Reuse

1. **Dual-interface converter nodes**: The pattern of implementing both a WCT interface
   (`IDepoSource`, `IFrameSink`, etc.) and a framework-facing interface simultaneously
   is sound. For `wire-cell-phlex`, replace `IArtEventVisitor` with PHLEX traits.

2. **Buffering strategy**: Converters buffer WCT data in member variables between the
   "receive from framework" and "serve to WCT" calls. This decoupling is necessary
   and should be preserved.

3. **WCT::Main as embedded executor**: The `WCLS` approach of instantiating one
   `WireCell::Main` and calling it per-event can be adapted to PHLEX.

4. **Component discovery by name**: Post-initialization lookup of converters by name
   (`Factory::find_tn`) allows clean configuration via string names.

5. **Parameter injection**: Passing PHLEX config values as WCT Jsonnet `extVar`s is
   a clean way to bridge the two configuration systems.

6. **Data conversion logic**: The actual `sim::SimEnergyDeposit → IDepo`,
   `raw::RawDigit → IFrame`, `IFrame → recob::Wire` conversions can be adapted;
   the WCT side of these conversions is directly reusable.

### Patterns That Must Change

1. **`IArtEventVisitor` / `visit(art::Event&)`**: This interface is art-specific and
   cannot exist in PHLEX. Replace with PHLEX input/output channels.

2. **Synchronous event loop**: The per-event `inputers → wcmain() → outputers` sequence
   assumes one event at a time. PHLEX may have multiple events in flight; the buffering
   strategy must be thread-safe.

3. **`art::Event::get()` / `art::Event::put()`**: Not available in PHLEX. Data arrives
   via function parameters (PHLEX products) and leaves via function return values.

4. **FHiCL configuration**: Replace with PHLEX/Jsonnet configuration.

5. **`art::make_tool`**: PHLEX has its own plugin loading mechanism.

6. **`art::ProducesCollector`**: The upfront output declaration needed by art is not
   required in PHLEX (outputs are typed by function return types).

### Architectural Implication

The fundamental difference is:

- **larwirecell**: art provides data via a central store → larwirecell feeds it
  into WCT, runs the WCT graph, pulls results out → art stores results.

- **wire-cell-phlex**: PHLEX products arrive as function arguments → wire-cell-phlex
  converts them, runs WCT (or is a WCT node itself), returns results → PHLEX routes
  the returned products downstream.

The "run entire WCT graph" approach is feasible but requires careful design of how
PHLEX products map to WCT source/sink nodes. An alternative is to not run the full
WCT graph internally, but instead surface individual WCT nodes as PHLEX operators.
See `design-options.md` for analysis.
