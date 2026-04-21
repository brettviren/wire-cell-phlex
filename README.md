# wire-cell-phlex (WCPh)

The `wire-cell-phlex` (WCPh) provides a general integration layer between the
[Wire-Cell Toolkit](https://wirecell.github.io/) (WCT) and the [PHLEX
framework](https://github.com/framework-r-d/phlex).  Through PHLEX plugin
libraries it allows WCT data flow programming (DFP) graphs to execute as a node
in the PHLEX DFP graph.

## Dependencies

| Dependency | Role |
|------------|------|
| [PHLEX](https://github.com/framework-r-d/phlex) ≥ GCC 15 | framework |
| [Wire-Cell Toolkit](https://github.com/WireCell/wire-cell-toolkit) | signal processing / simulation |
| C++23 (GCC 15) | compiler |

## Building

```sh
cmake --preset default          # configures with GCC 15, build dir = ./build
cmake --build build
ctest --test-dir build
```
All tests should pass.

## Package layout  overview

```
wire-cell-phlex/
├── cmake/            # build control
├── wire_cell_phlex/  # shared library source
├── modules/          # PHLEX MODULE shared libraries (dlopen only)
├── cfg/              # WCT Jsonnet sub-graph configs
├── test/             # ctest tests
└── docs/             # Documentation for humans and LLMs
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

## See also

See `docs/howto-new-workflow.md` for a step-by-step guide to writing a new workflow.
See `docs/design-options.md` for the integration architecture.



## License

Copyright © 2026, Brookhaven Science Associates, LLC.  All rights reserved.
Licensed under the [Apache License, Version 2.0](LICENSE).
See also `NOTICE`, `AUTHORS`, and `CONTRIBUTING`.
