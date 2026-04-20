# PHLEX Framework Summary

This document summarizes the PHLEX framework as found in `phlex/`, with emphasis on
information actionable for developing `wire-cell-phlex`.

## Repository Structure

```
phlex/
├── phlex/           # Core framework headers and sources
│   ├── core/        # Node declarations, graph proxy, registration API
│   ├── detail/      # Plugin macros, type utilities
│   └── metaprogramming/  # Type deduction utilities
├── form/            # ROOT I/O integration (optional)
├── plugins/         # Built-in plugins (generate_layers driver)
├── test/            # Unit and integration tests
├── docs/            # Documentation
└── CMakeLists.txt   # Cetmodules-based build
```

## Build System

- **Build system**: CMake with [Cetmodules](https://github.com/FNALsv/cetmodules) (FNAL's CMake extensions)
- **C++ standard**: C++23
- **Key dependencies**: TBB (flow graph), Boost (JSON, DLL, preprocessor), fmt, spdlog, jsonnet

**Produced libraries:**
| Library | Type | Purpose |
|---------|------|---------|
| `phlex_core` | shared | Main graph construction and execution |
| `phlex_model` | shared | Data model (product_store, handle, data_cell_index) |
| `phlex_metaprogramming` | header-only | Template type deduction utilities |
| `phlex_utilities` | shared | Helpers |
| `phlex_configuration_internal` | shared | Jsonnet/Boost.JSON config parsing |
| `phlex_module` | interface | Link target for plugin registration |
| `phlex_configuration` | public | Configuration API |
| `phlex_app` | executable | Application harness |

Plugin libraries are CMake `MODULE` targets (not `SHARED`), enabling dynamic loading.

## Execution Model

PHLEX implements a **Data Flow Programming (DFP)** paradigm using **TBB flow graphs**
as the underlying scheduler.

### Layer Hierarchy

Data is organized in a hierarchical structure of **layers**:
- A typical hierarchy might be: `job → run → event`
- The hierarchy is defined in the driver configuration
- Each layer has `data_cell_index` objects identifying individual cells (e.g., event number)
- Nodes operate at a specific layer level

### Node Types

PHLEX defines six fundamental node categories, registered via macros:

| Node Type | Registration Method | Signature | Purpose |
|-----------|---------------------|-----------|---------|
| **Provider** | `m.provide(name, func)` | `T(data_cell_index const&) → Product` | Data source at a layer |
| **Transform** | `m.transform(name, func)` | `(In...) → (Out...)` | Process data, produce output |
| **Observer** | `m.observe(name, func)` | `(In...) → void` | Side-effects, diagnostics |
| **Predicate** | `m.predicate(name, func)` | `(In...) → bool` | Filter data flow |
| **Fold** | `m.fold(name, func, serial, partition)` | `(&Accumulator, In) → void` | Accumulate over a layer range |
| **Unfold** | `m.unfold<Splitter>(name, pred, func, layer)` | creates child cells | Expand one cell into many children |
| **Output** | declared via `PHLEX_REGISTER_OUTPUTS` | `void(product_store const&)` | Terminal output (e.g., ROOT I/O) |

### Data Flow Topology

```
Driver (yields data_cell_index)
    ↓
Index Router (broadcasts to all entry nodes)
    ↓
Providers (produce products at each data cell)
    ↓
Transforms / Observers / Predicates / Folds / Unfolds
    ↓
Output nodes (write to persistent storage)
```

### Execution via TBB

- Nodes are wrapped as TBB `flow::graph` nodes
- `message` structs carry `{product_store_ptr, message_id}` along edges
- Parallelism controlled per-node via `concurrency::unlimited` or `concurrency::serial`
- Multi-input nodes use TBB join nodes, waiting for all inputs at the same `data_cell_index`

## Data Model

### Product Store

Each data cell (e.g., each event) has a `product_store` that holds all products created
for that cell. Products are keyed by `product_specification`:

```
product_specification = (plugin_name, algorithm_name, suffix, type_id)
```

### Product Handles

Functions receive data as:
- **Raw types**: `int const&`, `std::vector<T> const&` — simple value access
- **`handle<T>`**: Smart handle providing data plus metadata
  - `*handle` — access the data
  - `handle.layer()` — which layer this product is from
  - `handle.creator()` — which algorithm produced it
  - `handle.suffix()` — product suffix

### Product Queries

Input products are requested via `product_query`:
```cpp
product_query{
  .creator = "algorithm_name",  // required: who produced it
  .layer   = "event",           // required: at which layer
  .suffix  = "result",          // optional: disambiguation
  // type is auto-deduced from function parameter type
}
```

Matching is by `(creator, layer, suffix, type)` tuple. Products flow between nodes
through the framework's routing — not through a global store with explicit get/put.

## Plugin System

### Registration Macros

Plugins are `MODULE`-type shared libraries. They export entry point functions via
Boost.DLL symbol aliases. Three registration macros:

**`PHLEX_REGISTER_ALGORITHMS(m, config)`** — in algorithm (module) libraries:
```cpp
#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config) {
  m.transform("my_transform", my_function, concurrency::unlimited)
    .input_family(
      product_query{.creator = "source", .layer = "event", .suffix = "raw"})
    .output_product_suffixes("processed");
}
```

**`PHLEX_REGISTER_PROVIDERS(m, config)`** — in source (provider) libraries:
```cpp
#include "phlex/source.hpp"

PHLEX_REGISTER_PROVIDERS(m, config) {
  auto layer = config.get<std::string>("layer");
  m.provide("provide_data", [](data_cell_index const& id) -> MyData {
    return MyData{id.number()};
  }).output_product({.creator = "input", .layer = layer, .suffix = "data"});
}
```

**`PHLEX_REGISTER_DRIVER(d)`** — in driver libraries:
```cpp
#include "phlex/driver.hpp"

PHLEX_REGISTER_DRIVER(d) {
  return d.driver(
    fixed_hierarchy({{"job"}, {"job", "event"}}),
    [](data_cell_cursor const& job) {
      for (std::size_t i = 0; i < 100; ++i)
        job.yield_child("event", i);
    });
}
```

### Plugin Loading

The `cpp` field in Jsonnet config specifies the plugin library name. PHLEX loads
it dynamically and invokes the registered entry point.

## Configuration System

PHLEX uses Jsonnet for configuration. The top-level schema:

```jsonnet
{
  driver: {
    cpp: 'generate_layers',       // driver plugin library name
    layers: {
      run:   { parent: 'job',  total: 5  },
      event: { parent: 'run',  total: 100 },
    },
  },
  sources: {
    my_source: {
      cpp: 'my_source_library',   // provider plugin library name
      layer: 'event',
      // ... any config accessed via config.get<T>("key")
    },
  },
  modules: {
    my_module: {
      cpp: 'my_algorithm_library', // algorithm plugin library name
      layer: 'event',
      // ... algorithm-specific config
    },
  },
}
```

### Configuration Access in C++

Within registration macros, `config` is a `phlex::configuration` object (Boost.JSON wrapper):
```cpp
auto value   = config.get<std::string>("key");
auto maybe   = config.get_if_present<int>("optional_key");
auto nested  = config.get<phlex::configuration>("sub_object");
```

### Built-in Driver: `generate_layers`

The `generate_layers` plugin (in `phlex/plugins/`) generates a hierarchical layer
structure from the `driver.layers` config, eliminating the need to write a custom driver
for simple job/run/event hierarchies.

## Key Design Points for wire-cell-phlex

1. **No global event store**: PHLEX has no equivalent of `art::Event`. Data products
   flow through the graph via routing, not get/put on a central object.

2. **Products are typed at compile time**: The C++ type of a product is determined by
   the return type of the function that produces it. This is enforced at registration time.

3. **Stateless functions preferred**: Pure functions with no side effects are ideal.
   State can be captured in lambda closures at registration time (from `config`).

4. **Concurrency modes**: `concurrency::unlimited` allows parallel execution.
   `concurrency::serial` serializes access (required for folds with mutable state).

5. **Layer semantics**: "job"-layer data is produced once per job (e.g., geometry,
   calibration constants). "event"-layer data is produced once per event.

6. **Type deduction**: PHLEX uses `boost::callable_traits` to introspect function
   signatures and automatically wire up the graph. The developer just writes functions.

## Relevant Header Files

| Header | Purpose |
|--------|---------|
| `phlex/module.hpp` | `PHLEX_REGISTER_ALGORITHMS` macro |
| `phlex/source.hpp` | `PHLEX_REGISTER_PROVIDERS` macro |
| `phlex/driver.hpp` | `PHLEX_REGISTER_DRIVER` macro |
| `phlex/configuration.hpp` | `phlex::configuration` config wrapper |
| `phlex/concurrency.hpp` | `concurrency::unlimited`, `concurrency::serial` |
| `phlex/core/product_query.hpp` | `product_query` struct |
| `phlex/model/handle.hpp` | `handle<T>` smart handle |
| `phlex/model/data_cell_index.hpp` | `data_cell_index` hierarchy position |
| `phlex/model/product_store.hpp` | `product_store` per-cell product container |
| `phlex/model/fixed_hierarchy.hpp` | `fixed_hierarchy` layer topology definition |
