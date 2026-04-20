# PHLEX Examples Summary

This document summarizes the `phlex-examples/` package. It serves as the **reference
implementation** for how `wire-cell-phlex` should be organized, built, and written.

## Repository Structure

```
phlex-examples/
├── CMakeLists.txt              # Top-level build
├── README.md
├── my_add.hpp / my_add.cpp     # Framework-agnostic add library
├── my_geometry.hpp / my_geometry.cpp  # Framework-agnostic geometry library
├── numbers_source.cpp          # PHLEX provider module
├── geometry_source.cpp         # PHLEX provider module
├── add_and_verify.cpp          # PHLEX algorithm + observer module
├── ij_source.cpp               # PHLEX provider module (i,j integers)
├── module.cpp                  # Generic module example
├── test-cpp-workflow.jsonnet   # Test workflow config (C++)
├── test-py-workflow.jsonnet    # Test workflow config (Python)
├── subtract.py                 # Python algorithm module
└── migration/                  # LArSoft→PHLEX migration example
    ├── CMakeLists.txt
    └── gauss_hit_finder/       # Full GausHitFinder migration
```

## Build System Pattern

The CMakeLists.txt establishes the template for `wire-cell-phlex`:

```cmake
cmake_minimum_required(VERSION 3.30)
project(phlex_examples CXX)

find_package(phlex REQUIRED)
find_package(TBB REQUIRED)
find_package(Boost REQUIRED)

# 1. Framework-agnostic SHARED library (reusable algorithm implementation)
add_library(my_add SHARED my_add.cpp)
target_link_libraries(my_add PUBLIC ...)

# 2. PHLEX MODULE library (registration entry point, loaded dynamically)
add_library(add_and_verify MODULE add_and_verify.cpp)
target_link_libraries(add_and_verify PRIVATE phlex::module my_add)
```

**Critical distinction:**
- `SHARED` libraries: contain algorithm logic, can be linked by other code
- `MODULE` libraries: contain PHLEX registration code only, loaded dynamically by framework

`wire-cell-phlex` should follow this same two-library pattern: WCT interface logic
in a shared library, PHLEX registration in a module library.

## Concrete Module Patterns

### Pattern 1: Simple Provider (numbers_source.cpp)

```cpp
#include "phlex/source.hpp"

PHLEX_REGISTER_PROVIDERS(m, config) {
  auto const layer = config.get<std::string>("layer");
  m.provide("provide_i", [](phlex::data_cell_index const& id) -> int {
    return id.number();
  }).output_product({.creator = "input", .layer = layer, .suffix = "i"});

  m.provide("provide_j", [](phlex::data_cell_index const& id) -> int {
    return id.number() * 2;
  }).output_product({.creator = "input", .layer = layer, .suffix = "j"});
}
```

Key points:
- Provider lambda takes `data_cell_index const&` only (no other inputs)
- Return type determines the product type
- Output product tagged with `{creator, layer, suffix}` for routing

### Pattern 2: Geometry Provider (geometry_source.cpp)

```cpp
PHLEX_REGISTER_PROVIDERS(m, config) {
  auto geometry_name = config.get<std::string>("geometry_name");
  m.provide("provide_geometry",
    [geometry_name](phlex::data_cell_index const& /* job */) -> examples::geometry {
      return examples::geometry{geometry_name};
    }
  ).output_product({.creator = "input", .layer = "job", .suffix = "geometry"});
}
```

Key points:
- Job-layer provider: one geometry object per entire job
- Configuration captured in lambda closure at registration time (not per-event)
- Complex user-defined types can be products

### Pattern 3: Transform + Observer (add_and_verify.cpp)

```cpp
#include "phlex/module.hpp"
#include "my_add.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config) {
  auto const layer = config.get<std::string>("layer");

  // Transform: two inputs → one output
  m.transform("add", examples::add, concurrency::unlimited)
    .input_family(
      phlex::product_query{.creator = "input", .layer = layer, .suffix = "i"},
      phlex::product_query{.creator = "input", .layer = layer, .suffix = "j"})
    .output_product_suffixes("sum");

  // Observer: consumes, no output
  m.observe("verify", [](int sum) {
    // assert, log, etc.
  }, concurrency::unlimited)
    .input_family(
      phlex::product_query{.creator = "add", .layer = layer, .suffix = "sum"});
}
```

The underlying function is in a separate framework-agnostic file:
```cpp
// my_add.hpp / my_add.cpp
namespace examples {
  int add(int i, int j) { return i + j; }
}
```

### Pattern 4: Lambda Transform with Captured State (migration example)

```cpp
PHLEX_REGISTER_ALGORITHMS(m, config) {
  // Build algorithm objects from config ONCE at registration time
  auto cfg = parse_config(config);
  auto tool1 = std::make_shared<CandHitFinder>(config.get<configuration>("tool1_config"));
  auto tool2 = std::make_shared<PeakFitter>(config.get<configuration>("tool2_config"));

  // Lambda captures objects, called per-event with concurrency::unlimited
  m.transform("process",
    [cfg = std::move(cfg), tool1 = std::move(tool1), tool2 = std::move(tool2)]
    (std::vector<recob::Wire> const& wires) -> std::vector<recob::Hit> {
      return find_hits(cfg, wires, *tool1, *tool2);
    },
    concurrency::unlimited)
    .input_family(product_query{.creator = "wires", .layer = layer, .suffix = ""})
    .output_product_suffixes("hits");
}
```

This is the key pattern for wrapping stateful algorithm objects (like WCT components).

## Jsonnet Configuration Schema

### test-cpp-workflow.jsonnet

```jsonnet
{
  driver: {
    cpp: 'generate_layers',
    layers: {
      spill: { parent: 'job', total: 10, starting_number: 1 }
    },
  },
  sources: {
    geometry_source: {
      cpp: 'geometry_source',
      geometry_name: 'standard',     // passed to config.get<string>("geometry_name")
    },
    numbers: {
      cpp: 'numbers_source',
      layer: 'spill',                // passed to config.get<string>("layer")
    },
  },
  modules: {
    add_cpp: {
      cpp: 'add_and_verify',
      layer: 'spill',
    },
    output: {
      cpp: 'form_module',            // built-in ROOT output
      products: ['i', 'j', 'sum'],
    },
  },
}
```

### test-py-workflow.jsonnet (Python modules)

```jsonnet
{
  modules: {
    subtract: {
      py: 'subtract',               // Python module (not cpp)
      input: [
        { creator: 'input', suffix: 'i', layer: 'job' },
        { creator: 'input', suffix: 'j', layer: 'job' }
      ],
      output: ['difference'],
    },
  },
}
```

Python modules use `py:` instead of `cpp:` and specify input/output in config
rather than chained API.

## Migration Example: GausHitFinder

The `migration/` subdirectory demonstrates migrating a LArSoft/art-based algorithm
to PHLEX. It is highly relevant to `wire-cell-phlex` as both involve migrating
framework-coupled code.

**Architecture:**
```
gauss_hit_finder (SHARED library)
    ├── Algorithm implementation (pure C++, no framework dependency)
    ├── LArSoft data types (Hit, Wire, geo_types) — stand-alone copies
    └── Sub-tools (CandHitStandard, PeakFitterMrqdt, HitFilterAlg)

wires_source (MODULE library)
    └── PHLEX_REGISTER_PROVIDERS: reads Wire objects from files

find_hits_with_gaussians_hof (MODULE library)
    └── PHLEX_REGISTER_ALGORITHMS: wires up algorithm + tools
```

**Key migration lessons:**
1. **No framework geometry service**: geometry must be a PHLEX provider (job-layer)
2. **Tool objects**: instantiated in registration lambda, captured by move
3. **Inner parallelism**: algorithm may use `tbb::parallel_for` internally
4. **Data types**: LArSoft data types (Wire, Hit) included directly — no framework dependency

## Key Patterns for wire-cell-phlex

| Concern | phlex-examples Pattern | Applies to wire-cell-phlex? |
|---------|------------------------|------------------------------|
| Algorithm library | SHARED lib, no framework deps | Yes: WCT logic in shared lib |
| Registration | MODULE lib linking `phlex::module` | Yes: separate module per component group |
| Configuration | `config.get<T>("key")` at registration time | Yes: WCT Jsonnet path from config |
| Geometry/constants | Job-layer provider | Yes: WCT geometry loaded once |
| Per-event processing | Transform with unlimited concurrency | Yes: most WCT processing |
| Stateful algorithms | Lambda capture at registration | Yes: WCT component objects |
| Data types | Stand-alone C++ types, no framework | Complicated: WCT uses IData hierarchy |
| Output | `form_module` or custom observer | TBD: WCT output → PHLEX products |

The migration example's approach to `wires_source` (provider that reads from files)
is directly analogous to how `wire-cell-phlex` will need to adapt WCT `IDepoSource`
or other source nodes to serve as PHLEX providers.
