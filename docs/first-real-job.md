# First Real Job: DepoSetDrifter in PHLEX

This document describes the integration of a real WCT simulation component —
`DepoSetDrifter` — into a PHLEX workflow via wire-cell-phlex executors.  It
covers the 3-node drifter job, the 2-node identity round-trip, and explains
the two new executor patterns introduced.

## Overview

All previous wire-cell-phlex development (Steps 1–10) used synthetic in-process
test data.  This push connects real WCT I/O:

- **Source**: `DepoFileSource` reads a `.npz` file of ionisation deposits.
- **Filter**: `DepoSetDrifter` applies ionisation drift (diffusion, electron
  lifetime, x-region boundaries).
- **Sink**: `DepoFileSink` writes the drifted deposits to a `.npz` file.

Each of these WCT sub-graphs runs inside a PHLEX module; the PHLEX graph has
3 nodes connected in a linear chain.  A simpler 2-node identity variant
(source → sink, no drifter) validates the round-trip I/O.

## Quick start

```sh
# Build
cmake --preset default
cmake --build build

# Run all tests (includes identity and drifter)
ctest --test-dir build
```

The integration tests write their output to the build directory:
- `build/identity-output.npz` — round-trip copy of the input
- `build/drifted-output.npz` — drift-simulated deposits

## 3-node drifter job

```
PHLEX workflow
┌───────────────────────┐     ┌───────────────────────┐     ┌──────────────────────────┐
│  wcph_deposet_source_  │     │   wcph_deposet_filter  │     │  wcph_deposet_sink_file   │
│  file                 │────▶│   (DepoSetDrifter)    │────▶│                          │
│  (DepoFileSource)     │     │                       │     │  (DepoFileSink)           │
└───────────────────────┘     └───────────────────────┘     └──────────────────────────┘
  reads muon-depos.npz          drifts all depositions         writes drifted-output.npz
```

Inside each PHLEX module a complete WCT sub-graph runs:

| PHLEX module | WCT sub-graph | Jsonnet config |
|---|---|---|
| `wcph_deposet_source_file` | `DepoFileSource → DepoSetBoundarySink` | `deposet-file-source.jsonnet` |
| `wcph_deposet_filter` | `DepoSetBoundarySource → DepoSetDrifter → DepoSetBoundarySink` | `deposet-drifter.jsonnet` |
| `wcph_deposet_sink_file` | `DepoSetBoundarySource → DepoFileSink` | `deposet-file-sink.jsonnet` |

## 2-node identity job

```
PHLEX workflow
┌───────────────────────┐     ┌──────────────────────────┐
│  wcph_deposet_source_  │     │  wcph_deposet_sink_file   │
│  file                 │────▶│                          │
│  (DepoFileSource)     │     │  (DepoFileSink)           │
└───────────────────────┘     └──────────────────────────┘
  reads muon-depos.npz           writes identity-output.npz
```

The source product has `creator = "input"` (hardcoded in the source module),
so the sink's `input_from` key must be `"input"`.

## New executor patterns

### `DepoSetFilter` (IDepoSet → IDepoSet)

Mirrors `FrameFilter` exactly.  Per-event fill/run/drain cycle:
1. Fill `DepoSetBoundarySource` with input depo set.
2. Run WCT graph via `m_wcmain()`.
3. Drain `DepoSetBoundarySink` for the output depo set.

Use this for any WCT `IDepoSetFilter` component (e.g. `DepoSetDrifter`).

### `DepoSetSourceFile` (file → IDepoSet)

New pattern — no `BoundarySource`.  The WCT source component (`DepoFileSource`)
reads the file internally and produces all depo sets in a single graph run.

- **First `operator()()` call**: initializes WCT, runs graph to completion.
  All depo sets queue in `DepoSetBoundarySink`.
- **Subsequent calls**: drain one depo set per call.

The PHLEX workflow `total` must match the number of depo sets in the file
(e.g., `total: 1` for `muon-depos.npz` which contains one depo set).

### `DepoSetSinkFile` (IDepoSet → file)

New pattern — no `BoundarySink`.  The WCT sink component (`DepoFileSink`)
writes the file internally.

- **Each `operator()(DepoSet)` call**: fills `DepoSetBoundarySource`, runs graph.
  `DepoFileSink` accumulates all events.
- **Job end**: `WireCell::Main::~Main()` automatically calls `finalize()` on all
  `ITerminal` components, flushing and closing the output file.

**Important**: do NOT call `m_wcmain.finalize()` explicitly in the destructor —
`Main::~Main()` already calls it, causing a double-finalize that triggers a
`boost::iostreams::chain::pop()` assertion on an empty chain.

## Configuration reference

### `wcph_deposet_source_file`

| Key | Type | Default | Description |
|---|---|---|---|
| `wct_config` | string | required | Path to WCT Jsonnet (e.g. `deposet-file-source.jsonnet`) |
| `output_layer` | string | required | PHLEX layer for output `DepoSet` product |
| `wct_plugins` | string[] | `[]` | Must include `WireCellPgraph`, `WireCellSio` |
| `wct_tla` | object | `{}` | Extra Jsonnet TLAs; use `{ inname: "path/to/depos.npz" }` |

### `wcph_deposet_filter`

| Key | Type | Default | Description |
|---|---|---|---|
| `wct_config` | string | required | Path to WCT Jsonnet (e.g. `deposet-drifter.jsonnet`) |
| `input_layer` | string | required | PHLEX layer for input `DepoSet` product (creator = module label or `"input"`) |
| `wct_plugins` | string[] | `[]` | Must include `WireCellPgraph`; add `WireCellGen` for drifter |
| `wct_tla` | object | `{}` | Extra Jsonnet TLAs forwarded to WCT config |

### `wcph_deposet_sink_file`

| Key | Type | Default | Description |
|---|---|---|---|
| `wct_config` | string | required | Path to WCT Jsonnet (e.g. `deposet-file-sink.jsonnet`) |
| `input_layer` | string | required | PHLEX layer of input `DepoSet` product |
| `input_from` | string | required | Creator name of upstream product (`"input"` from source_file, or module label from filter) |
| `wct_plugins` | string[] | `[]` | Must include `WireCellPgraph`, `WireCellSio` |
| `wct_tla` | object | `{}` | Extra Jsonnet TLAs; use `{ outname: "path/to/output.npz" }` |

## Drifter physics configuration

The `deposet-drifter.jsonnet` uses fictional single-APA parameters for testing:

```jsonnet
xregions: [{ anode: 0.0, response: 10*wc.cm, cathode: 3.6*wc.m }]
DL:          7.2 * wc.cm2 / wc.s   // longitudinal diffusion
DT:          12.0 * wc.cm2 / wc.s  // transverse diffusion
lifetime:    8 * wc.ms              // electron lifetime
drift_speed: 1.6 * wc.mm / wc.us   // nominal LAr drift speed
fluctuate:   false                   // deterministic (no Poisson noise)
```

Replace `xregions` with real detector geometry for production use.  Each
x-region entry must have `anode`, `response`, and `cathode` as scalar X
coordinates.

## Test data

`test/data/muon-depos.npz` contains one depo set with 32,825 depositions
from a simulated muon.  Copied from `wire-cell-toolkit/test/data/`.

## WCT plugin loading

The required plugins for each workflow:

| Workflow | Plugins |
|---|---|
| identity (source + sink only) | `WireCellPgraph`, `WireCellSio` |
| drifter (source + filter + sink) | `WireCellPgraph`, `WireCellSio`, `WireCellGen` |

Each PHLEX module loads plugins independently via its `wct_plugins` config key.

## PHLEX TLA limitation

PHLEX 0.2.0 does not support command-line Jsonnet TLAs (`--tla-str`) or
external variables (`--ext-str`).  File paths are injected into workflow Jsonnet
files at build time using CMake's `configure_file()`.  The `.jsonnet.in`
templates in `test/` use `@WCP_TEST_DATA_DIR@` and `@WCP_OUTPUT_DIR@` as
substitution markers.

## Adapting to other WCT components

To run a different `IDepoSetFilter` component:

1. Write a WCT Jsonnet config with:
   ```jsonnet
   function(source_name="...", sink_name="...", app_name="...")
   [ {type:"DepoSetBoundarySource", name:source_name, ...},
     {type:"MyFilter", ...},
     {type:"DepoSetBoundarySink",   name:sink_name,   ...},
     {type:"Pgrapher", name:app_name, data:{edges:[...]}} ]
   ```
2. Use `wcph_deposet_filter` in the PHLEX workflow with `wct_config: "my-filter.jsonnet"`.

For a different file-based source, replace `DepoFileSource` in
`deposet-file-source.jsonnet` with the desired `IDepoSetSource` component.
Set `wct_plugins` to include whichever WCT plugin provides the new component.
