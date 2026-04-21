# wire-cell-phlex

Integration of the [Wire-Cell Toolkit](https://wirecell.github.io/) (WCT) with the
[PHLEX framework](https://github.com/framework-r-d/phlex).

`wire-cell-phlex` provides PHLEX module libraries that wrap WCT signal-processing and
simulation sub-graphs as `concurrency::serial` PHLEX transform nodes.  Data products
are exchanged using thin aggregate wrapper types (`wcphlex::Frame`, `wcphlex::DepoSet`,
…) that carry WCT's immutable `shared_ptr<const IData>` objects through PHLEX's typed
product routing.

See `docs/howto-new-workflow.md` for a step-by-step guide to writing a new workflow.
See `docs/design-options.md` for the integration architecture.

## Dependencies

| Dependency | Role |
|------------|------|
| [PHLEX](https://github.com/framework-r-d/phlex) ≥ GCC 15 | framework |
| [Wire-Cell Toolkit](https://github.com/WireCell/wire-cell-toolkit) | signal processing / simulation |
| Boost (json, iostreams, …) | WCT / PHLEX transitive |
| Eigen3 | WCT transitive |
| jsoncpp / jsonnet | WCT configuration |
| spdlog | logging |
| TBB | PHLEX scheduler |

PHLEX headers use `std::forward_like` (C++23), which requires **GCC 15** or Clang 18+.
The `wire_cell_phlex` shared library is header-only against PHLEX's core types, so it
can be compiled with GCC 12+, but the PHLEX MODULE files must use GCC 15.

## Building

Activate the Spack environment that provides PHLEX and WCT, then:

```sh
cmake --preset default          # configures with GCC 15, build dir = ./build
cmake --build build
ctest --test-dir build
```

All 9 tests should pass:

| Test | What it exercises |
|------|-------------------|
| `trivial` | bare WCT link check |
| `data_types` | wcphlex wrapper type IDs |
| `boundary` | BoundarySource/Sink buffers |
| `executor` | FrameFilter unit test (12 events) |
| `phlex_frame_filter` | PHLEX: frame source → passthrough → observer |
| `phlex_deposet_to_frame` | PHLEX: DepoSet source → trivial framer → observer |
| `phlex_multi_instance` | PHLEX: two independent FrameFilter instances |
| `phlex_wire_schema` | PHLEX: job-layer wire geometry load + validation |
| `phlex_frame_filter_facade` | PHLEX: FacadeWireSchema geometry bridge |

## Package layout

```
wire-cell-phlex/
├── CMakeLists.txt
├── cmake/
│   └── FindWireCell.cmake        # locate WCT libraries from CMake
├── wire_cell_phlex/              # SHARED library (wire_cell_phlex)
│   ├── Data.h                    # wcphlex wrapper types
│   ├── BoundarySource.h          # WCT source buffer template
│   ├── BoundarySink.h            # WCT sink buffer template
│   ├── Executor.h / .cpp         # WctExecutor base + FrameFilter, DepoSetToFrame
│   ├── BoundaryNodes.cpp         # WIRECELL_FACTORY registrations
│   ├── TrivialDepoFramer.cpp     # minimal IDepoSet→IFrame for testing
│   ├── FacadeWireSchema.h / .cpp # IWireSchema bridge for PHLEX job-layer geometry
│   └── Data.h                    # wcphlex wrapper types
├── modules/                      # PHLEX MODULE shared libraries (dlopen only)
│   ├── frame_source.cpp          # wcp_frame_source:   event-layer Frame provider
│   ├── frame_filter.cpp          # wcp_frame_filter:   Frame → Frame transform
│   ├── frame_observer.cpp        # wcp_frame_observer: single-frame validation
│   ├── two_frame_observer.cpp    # wcp_two_frame_observer: pair validation
│   ├── deposet_source.cpp        # wcp_deposet_source:     event-layer DepoSet
│   ├── deposet_to_frame.cpp      # wcp_deposet_to_frame:   DepoSet → Frame
│   ├── wire_schema_source.cpp    # wcp_wire_schema_source:  job-layer geometry
│   ├── wire_schema_observer.cpp  # wcp_wire_schema_observer: geometry validation
│   └── executor_config.h         # phlex::configuration → boost::json helper
├── cfg/                          # WCT Jsonnet sub-graph configs
│   ├── frame-passthrough.jsonnet               # trivial Frame pass-through
│   └── frame-passthrough-with-facade.jsonnet   # pass-through + FacadeWireSchema
├── test/
│   ├── CMakeLists.txt
│   ├── test_trivial.cpp
│   ├── test_data_types.cpp
│   ├── test_boundary.cpp
│   ├── test_executor.cpp
│   ├── frame-filter-workflow.jsonnet
│   ├── deposet-to-frame-workflow.jsonnet
│   ├── multi-instance-workflow.jsonnet
│   ├── wire-schema-workflow.jsonnet
│   └── frame-filter-facade-workflow.jsonnet
└── docs/
    ├── howto-new-workflow.md     # step-by-step guide for adding a new workflow
    ├── design-options.md         # integration architecture options considered
    ├── implementation-plan.md    # phased build-out plan (Steps 1–10)
    └── *.md                      # WCT/PHLEX reference summaries
```

## PHLEX modules reference

### Providers (sources)

| Module (`cpp` key) | Output product | Config keys |
|--------------------|---------------|-------------|
| `wcp_frame_source` | `wcphlex::Frame` at event layer | `output_layer`, `output_suffix` (default `"frame"`) |
| `wcp_deposet_source` | `wcphlex::DepoSet` at event layer | `output_layer` |
| `wcp_wire_schema_source` | `wcphlex::WireSchema` at job layer | `output_layer`, `wire_schema_file` |

### Transforms / observers

| Module (`cpp` key) | Description | Key config keys |
|--------------------|-------------|-----------------|
| `wcp_frame_filter` | Runs any WCT IFrame→IFrame graph | `wct_config`, `input_layer`, `input_suffix`, `use_wire_schema`, `wire_schema_layer` |
| `wcp_deposet_to_frame` | Runs any WCT IDepoSet→IFrame graph | `wct_config`, `input_layer` |
| `wcp_frame_observer` | Asserts output Frame non-null | `input_layer`, `input_from` |
| `wcp_two_frame_observer` | Asserts two Frames non-null + distinct | `input_layer`, `input_from_a`, `input_from_b` |
| `wcp_wire_schema_observer` | Asserts WireSchema loaded + non-empty | `input_layer` |

### Common executor config keys (`wcp_frame_filter`, `wcp_deposet_to_frame`)

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `wct_config` | string | required | Path to WCT Jsonnet config (searched via `WIRECELL_PATH`) |
| `wct_plugins` | string[] | `[]` | Extra WCT plugin libraries (e.g. `["WireCellPgraph"]`) |
| `wct_app` | string | `"Pgrapher"` | WCT IApplication type |
| `wct_tla` | object | `{}` | Extra Jsonnet top-level arguments |
| `use_wire_schema` | bool | `false` | Consume job-layer WireSchema for FacadeWireSchema bridge |
| `wire_schema_layer` | string | `"job"` | Layer to read WireSchema from (when `use_wire_schema` is true) |

## Running test workflows manually

Set `PHLEX_PLUGIN_PATH` to the build directory (and optionally `WIRECELL_PATH`):

```sh
export PHLEX_PLUGIN_PATH=${PWD}/build:$(phlex-lib-dir)
export WIRECELL_PATH=${PWD}/cfg

# Basic frame passthrough (3 events)
phlex -c test/frame-filter-workflow.jsonnet

# Two independent filter instances on separate streams
phlex -c test/multi-instance-workflow.jsonnet

# Job-layer geometry load + validation
WIRECELL_PATH=${PWD}/cfg:/path/to/wirecell/share/wirecell \
  phlex -c test/wire-schema-workflow.jsonnet

# Geometry bridge via FacadeWireSchema
WIRECELL_PATH=${PWD}/cfg:/path/to/wirecell/share/wirecell \
  phlex -c test/frame-filter-facade-workflow.jsonnet
```

## License

Copyright © 2026, Brookhaven Science Associates, LLC.  All rights reserved.
Licensed under the [Apache License, Version 2.0](LICENSE).
See also `NOTICE`, `AUTHORS`, and `CONTRIBUTING`.
