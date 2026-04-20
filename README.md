# wire-cell-phlex

Integration of the [Wire-Cell Toolkit](https://wirecell.github.io/) (WCT) with the
[PHLEX framework](https://github.com/framework-r-d/phlex).

`wire-cell-phlex` provides PHLEX module libraries that wrap WCT signal-processing and
simulation sub-graphs as `concurrency::serial` PHLEX transform nodes.  Data products
are exchanged using thin aggregate wrapper types (`wcphlex::Frame`, `wcphlex::DepoSet`,
…) that carry WCT's immutable `shared_ptr<const IData>` objects through PHLEX's
typed product routing.

See `docs/design-options.md` for the integration architecture and
`docs/implementation-plan.md` for the phased build-out.

## Dependencies

| Dependency | Role |
|------------|------|
| [PHLEX](https://github.com/framework-r-d/phlex) | framework |
| [Wire-Cell Toolkit](https://github.com/WireCell/wire-cell-toolkit) | signal processing / simulation |
| Boost (graph, headers, iostreams, …) | WCT / PHLEX transitive |
| Eigen3 | WCT transitive |
| jsoncpp / jsonnet | WCT configuration |
| spdlog | logging |
| TBB | PHLEX scheduler |

## Building

Activate the environment that provides PHLEX and WCT, then:

```sh
cmake --preset default -B build
cmake --build build
ctest --test-dir build
```

## Running

Add the build directory to `PHLEX_PLUGIN_PATH` so PHLEX can locate the MODULE
libraries, then run a workflow:

```sh
export PHLEX_PLUGIN_PATH=${PWD}/build:${PHLEX_PLUGIN_PATH}
phlex -c test/frame-filter-workflow.jsonnet   # available from Step 5
```

## Package layout

```
wire-cell-phlex/
├── CMakeLists.txt
├── cmake/
│   └── FindWireCell.cmake       # locates WCT from CMake
├── wire_cell_phlex/             # SHARED library: integration layer
│   ├── Data.h                   # wcphlex wrapper types (Step 2)
│   ├── BoundarySource.h         # WCT source buffer template (Step 3)
│   ├── BoundarySink.h           # WCT sink buffer template (Step 3)
│   └── Executor.h / .cpp        # WctExecutor (Step 4)
├── frame_filter.cpp             # PHLEX MODULE: Frame → Frame (Step 5)
├── depo_to_frame.cpp            # PHLEX MODULE: DepoSet → Frame (Step 6)
├── cfg/                         # WCT Jsonnet sub-graph configs
├── test/                        # unit and integration tests
└── docs/                        # design documents and summaries
```

# License, etc

This project is Copyright (c) 2026, Brookhaven Science Associates, LLC.  All
rights reserved.  Licensed under the Apache License, Version 2.0.  See the
LICENSE file for details.

See also the files NOTICE, AUTHORS and CONTRIBUTING.
