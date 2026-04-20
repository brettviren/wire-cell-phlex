# Wire-Cell Toolkit (WCT) Summary

This document summarizes the Wire-Cell Toolkit as found in `wire-cell-toolkit/`,
focusing on integration-relevant aspects for `wire-cell-phlex`.

## Repository Structure

```
wire-cell-toolkit/
├── util/        # Core utilities: NamedFactory, PluginManager, ConfigManager, IComponent
├── iface/       # All interface headers: INode, IData subclasses, IConfigurable, etc.
├── aux/         # Auxiliary helpers: Logger, SimpleDepo, SimpleFrame, etc.
├── apps/        # Main entry point, CLI
├── pgraph/      # Pgrapher: single-threaded DFP graph executor
├── tbb/         # TbbFlow: multi-threaded DFP graph executor
├── gen/         # Generator plugins (physics simulation)
├── sigproc/     # Signal processing plugins
├── img/         # Imaging/tomography plugins
├── cfg/         # Jsonnet configuration library (wirecell.jsonnet, pgraph.jsonnet)
└── ...          # Other plugin packages
```

Each sub-directory is a sub-package producing a shared library (e.g., `libWireCellUtil.so`).

## Build System

WCT uses its own build system called **waf** (via the `wcb` script). It is **not CMake**.
However, `wire-cell-phlex` itself should use CMake (following phlex-examples), and
will need to find WCT libraries via a CMake `FindWireCell.cmake` module.

The `larwirecell/Modules/FindWireCell.cmake` provides a reference implementation of
how to locate WCT from CMake.

## Component and Plugin System

### IComponent / CRTP Pattern

All major WCT interfaces use CRTP via `IComponent<Self>`:

```cpp
template <class Type>
class IComponent : virtual public Interface {
  typedef std::shared_ptr<Type> pointer;
  typedef std::vector<pointer> vector;
};

class IMyInterface : public IComponent<IMyInterface> {
  // pure virtual methods
};
```

This provides `IMyInterface::pointer` (= `std::shared_ptr<IMyInterface>`) universally.

### Plugin Registration Macro

WCT plugins register themselves with:

```cpp
// In a .cxx source file:
WIRECELL_FACTORY(MyNode, WireCell::Pkg::MyNode,
                 WireCell::IFunctionNode<WireCell::IFrame, WireCell::IFrame>,
                 WireCell::IConfigurable)
```

This macro:
1. Registers `MyNode` in the global `NamedFactoryRegistry` for each listed interface
2. Exports a C symbol `make_MyNode_factory()` for dynamic loading

### Plugin Loading and Lookup

```cpp
// Load a plugin library
WireCell::PluginManager::instance().add("WireCellGen");
// → loads libWireCellGen.so

// Look up a component by class and instance name
auto node = WireCell::Factory::lookup<IFunctionNode<IFrame,IFrame>>("MyNode", "myname");

// Look up via "type:name" string
auto node = WireCell::Factory::lookup_tn<INode>("MyNode:myname");

// Find existing (do not create)
auto node = WireCell::Factory::find<IConfigurable>("MyNode", "myname");
```

## Node Hierarchy (iface/)

### Base Node Interface

```cpp
// iface/inc/WireCellIface/INode.h
class INode : public IComponent<INode> {
public:
  enum NodeCategory {
    sourceNode, sinkNode, functionNode,
    queuedoutNode, joinNode, splitNode,
    faninNode, fanoutNode, multioutNode, hydraNode
  };
  virtual NodeCategory category() = 0;
  virtual std::string signature() = 0;
  virtual int concurrency() { return 1; }
  virtual std::vector<std::string> input_types()  { return {}; }
  virtual std::vector<std::string> output_types() { return {}; }
};
```

### Key Node Templates

| Interface | Template | Operator Signature |
|-----------|----------|--------------------|
| `ISourceNode<T>` | `ISourceNode<OutputType>` | `bool operator()(output_pointer& out)` |
| `ISinkNode<T>` | `ISinkNode<InputType>` | `bool operator()(const input_pointer& in)` |
| `IFunctionNode<I,O>` | `IFunctionNode<In,Out>` | `bool operator()(const input_pointer& in, output_pointer& out)` |
| `IFaninNode<I,O,N>` | `IFaninNode<In,Out,N>` | N inputs → 1 output |
| `IFanoutNode<I,O,N>` | `IFanoutNode<In,Out,N>` | 1 input → N outputs |
| `IQueuedOutNode<I,O>` | `IQueuedOutNode<In,Out>` | 1 input → queue of outputs |
| `IJoinNode<IN,OUT>` | `IJoinNode<InputTuple,Out>` | tuple of inputs → 1 output |
| `ISplitNode<I,OUT>` | `ISplitNode<In,OutputTuple>` | 1 input → tuple of outputs |

All return `bool`: `true` if data was produced/consumed, `false` for end-of-stream (EOS).

### EOS Protocol

WCT uses `nullptr` (null `shared_ptr`) as the end-of-stream sentinel:
```cpp
bool operator()(const input_pointer& in, output_pointer& out) {
  if (!in) {        // EOS received
    out = nullptr;  // propagate EOS
    return true;    // return true to indicate handled
  }
  // ... process data
}
```

## Data Type Hierarchy (iface/)

### IData Base

```cpp
template <class Type>
class IData {
  typedef std::shared_ptr<const Type> pointer;
  typedef std::vector<pointer> vector;
  typedef std::shared_ptr<const vector> shared_vector;
};
```

All WCT data objects are:
- **Immutable**: `shared_ptr<const Type>` — no modification after creation
- **Heap-allocated**: always via `shared_ptr`
- **Reference-counted**: safe to share across threads

### Key Data Types

| Interface | File | Key Methods | Description |
|-----------|------|-------------|-------------|
| `IDepo` | `IDepo.h` | `pos()`, `time()`, `charge()`, `energy()`, `id()`, `pdg()`, `prior()` | Single ionization deposit (charge + position + time) |
| `IDepoSet` | `IDepoSet.h` | `ident()`, `depos()` | Collection of IDepo objects |
| `ITrace` | `ITrace.h` | `channel()`, `tbin()`, `charge()` | Single-channel waveform (ADC/charge values) |
| `IFrame` | `IFrame.h` | `traces()`, `ident()`, `time()`, `tick()`, `frame_tags()`, `tagged_traces(tag)` | Multi-channel frame (collection of ITrace) |
| `IWire` | `IWire.h` | `ident()`, `planeid()`, `index()`, `channel()`, `ray()` | Physical wire segment |
| `IChannel` | `IChannel.h` | `ident()`, `index()`, `segments()` | Electronics channel |
| `ITensor` | `ITensor.h` | `shape()`, `element_type()`, `data()`, `metadata()` | N-dimensional array |
| `ITensorSet` | `ITensorSet.h` | `tensors()`, `metadata()` | Collection of ITensor |
| `ICluster` | `ICluster.h` | Graph of charge blobs | 3D cluster |

### Important Derived Types

Nodes are often typed specifically:

| Interface | Inherits | Description |
|-----------|----------|-------------|
| `IDepoSource` | `ISourceNode<IDepo>` | Source of ionization deposits |
| `IDepoSink` | `ISinkNode<IDepo>` | Consumer of deposits |
| `IDrifter` | `IFunctionNode<IDepo,IDepo>` | Drift simulation |
| `IFrameSource` | `ISourceNode<IFrame>` | Source of ADC frames |
| `IFrameSink` | `ISinkNode<IFrame>` | Consumer of ADC frames |
| `IFrameFilter` | `IFunctionNode<IFrame,IFrame>` | Frame processing |
| `IDepoFilter` | `IFunctionNode<IDepo,IDepo>` | Deposition filtering |
| `IDepoSetFilter` | `IFunctionNode<IDepoSet,IDepoSet>` | Batch filtering |

## Configuration System (IConfigurable)

### Interface

```cpp
// iface/inc/WireCellIface/IConfigurable.h
class IConfigurable : virtual public IComponent<IConfigurable> {
  virtual Configuration default_configuration() const { return Configuration(); }
  virtual void configure(const WireCell::Configuration& cfg) = 0;
};
```

`Configuration` is `Json::Value` (from JsonCPP). It is loaded from Jsonnet via
`WireCell::Persist::load(filename)` or `WireCell::Persist::Parser`.

### Configuration Schema

WCT configuration is a JSON array of typed objects:
```json
[
  { "type": "Pgrapher", "name": "", "data": { "edges": [...] } },
  { "type": "TrackDepos", "name": "tracks", "data": { "step_size": 1.0 } },
  { "type": "ImpactTransform", "name": "sig", "data": { ... } }
]
```

### Jsonnet Configuration Library

`cfg/wirecell.jsonnet` provides:
- Physical units: `wc.mm`, `wc.us`, `wc.MeV`, etc.
- Helper: `wc.tn(obj)` → `"type:name"` string from config object
- Component constructor patterns

`cfg/pgraph.jsonnet` provides:
- Graph construction functions: `pg.edge(tail, head)`, `pg.intern(...)`, `pg.pipeline(...)`, `pg.fan.inn(...)`, `pg.fan.out(...)`
- High-level graph composition utilities

## WCT Initialization: WireCell::Main

The primary entry point for embedding WCT in a larger application is
`WireCell::Main` (`apps/inc/WireCellApps/Main.h`):

```cpp
#include <WireCellApps/Main.h>

WireCell::Main wcmain;

// Setup (before initialize):
wcmain.add_plugin("WireCellGen");       // load libWireCellGen.so
wcmain.add_plugin("WireCellSigProc");
wcmain.add_config("myconfig.jsonnet");  // load Jsonnet config file
wcmain.add_var("detector", "dune");     // Jsonnet external variable
wcmain.add_code("geometry", "...");     // Jsonnet external code snippet
wcmain.add_path("/path/to/cfg/");       // config search path
wcmain.add_app("Pgrapher");             // which WCT app to run

// Execute:
wcmain.initialize();   // parse configs, load plugins, instantiate components, configure
wcmain();              // execute all apps (runs the DFP graph)
wcmain.finalize();     // cleanup
```

### Initialization Flow (Main::initialize())

1. Parse all Jsonnet config files (with external vars applied)
2. Load all plugin libraries (adds to PluginManager)
3. For each configured component: instantiate via NamedFactory
4. For each component implementing `INamed`: call `set_name()`
5. For each component implementing `IConfigurable`:
   - Merge default config with user config
   - Call `configure()`
6. Find the apps (e.g., Pgrapher) — these are separate IApplication objects

### Execution Flow (Main::operator()())

1. Find each `IApplication` by name
2. Call `app->execute()` — for Pgrapher, this runs the DFP graph

### Pgrapher Execution (pgraph/Graph)

The Pgrapher uses pull-based execution:
1. Builds a topological sort (Kahn's algorithm)
2. Iterates: calls each node in topological order
3. Nodes pull from upstream queues, push to downstream queues
4. Queue entries are `boost::any` (type-erased `shared_ptr<const T>`)
5. Continues until no progress (all nodes return false / no data)

## Graph Communication: Type Erasure

WCT graph edges carry `boost::any`, which wraps `shared_ptr<const T>`:

```
ISourceNode<IFrame>::operator()  →  output_pointer (shared_ptr<const IFrame>)
                                 →  boost::any (edge queue entry)
                                 →  input_pointer (shared_ptr<const IFrame>)
  ↓
IFunctionNode<IFrame,IFrame>::operator()(const input_pointer& in, ...)
```

The wrapper classes in `pgraph/Wrappers.h` perform the `shared_ptr ↔ boost::any`
conversions. Port compatibility is checked via C++ `typeid().name()` strings.

## Defining a Custom WCT Node

```cpp
// MyNode.h
#include <WireCellIface/IFunctionNode.h>
#include <WireCellIface/IConfigurable.h>
#include <WireCellAux/Logger.h>

class MyNode : public WireCell::Aux::Logger,
               public WireCell::IFunctionNode<WireCell::IFrame, WireCell::IFrame>,
               public WireCell::IConfigurable {
public:
  MyNode();

  // IFunctionNode
  bool operator()(const input_pointer& in, output_pointer& out) override;

  // IConfigurable
  WireCell::Configuration default_configuration() const override;
  void configure(const WireCell::Configuration& cfg) override;
};

// MyNode.cxx
#include <WireCellUtil/NamedFactory.h>
WIRECELL_FACTORY(MyNode, MyNode,
                 WireCell::IFunctionNode<WireCell::IFrame, WireCell::IFrame>,
                 WireCell::IConfigurable)
```

## Key Integration Points for wire-cell-phlex

### Embedding WCT

The core question for `wire-cell-phlex` is how to bridge PHLEX's DFP model with WCT's
DFP model. Key facts:

1. **WCT::Main can be embedded**: Call `initialize()`, `operator()()`, `finalize()`.
   This executes the entire WCT graph once.

2. **Components accessible by name**: After `initialize()`, any WCT component can be
   found via `Factory::find<IFoo>("TypeName", "instance_name")`.

3. **Custom nodes can be injected**: A node that implements a WCT interface but gets
   its data from PHLEX (rather than another WCT node) can be registered before
   `initialize()` and will be incorporated into the WCT graph.

4. **No global clock/event**: WCT nodes are connected and called; there's no separate
   "event loop" concept — the graph IS the event loop.

### Relevant Libraries to Link

```cmake
target_link_libraries(wire_cell_phlex_module PRIVATE
  WireCellApps    # Main class
  WireCellIface   # INode, IData interfaces
  WireCellAux     # Logger, SimpleDepo, etc.
  WireCellUtil    # Factory, PluginManager, Configuration
  phlex::module   # PHLEX registration
)
```

### Configuration Integration Challenge

WCT uses Jsonnet with its own schema. PHLEX also uses Jsonnet with its own schema.
These schemas are incompatible. Possible approaches:
1. Pass WCT Jsonnet config file **paths** as PHLEX config values
2. Embed WCT config as a nested object within PHLEX config
3. Generate WCT config programmatically from PHLEX config values

See `wire-cell-phlex/docs/design-options.md` for detailed discussion.
