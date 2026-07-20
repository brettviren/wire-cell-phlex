# Second Real Job: DepoTransform (sim) in PHLEX

This document describes the integration of the WCT drift + electronics simulation
pipeline — `DepoSetDrifter` followed by `DepoTransform` — into a PHLEX workflow via
wire-cell-phlex executors.  It adds a new `FrameSinkFile` executor to receive the
output frames and write them to disk.

## Overview

The first real job proved out file-based DepoSet source/sink executors.  This push
extends the chain with `DepoTransform` — the WCT component that converts drifted
depositions into digitised readout frames.  The result is a 3-node pipeline:

```
┌───────────────────────┐    ┌───────────────────────────────┐    ┌──────────────────────┐
│ wcph_deposet_source_   │    │ wcph_deposet_to_frame          │    │ wcph_frame_sink_file  │
│ file                  │───▶│ (Drifter + DepoTransform)     │───▶│ (FrameFileSink)      │
│ reads muon-depos.npz  │    │                               │    │ writes sim-frames.npz│
└───────────────────────┘    └───────────────────────────────┘    └──────────────────────┘
```

The left and middle nodes reuse existing executors (`DepoSetSourceFile` and
`DepoSetToFrame`).  Only the WCT Jsonnet config for the middle node changes —
from the trivial `deposet-to-frame.jsonnet` to the new `deposet-drift-sim.jsonnet`.
The right node uses a new `FrameSinkFile` executor analogous to `DepoSetSinkFile`.

## Quick start

```sh
cmake --preset default
cmake --build build
ctest --test-dir build -R phlex_deposet_sim
```

The integration test writes `build/sim-frames.npz` — a WCT "frame file" containing
Numpy arrays of simulated wire-readout frames.

## WCT sub-graph inside `wcph_deposet_to_frame`

```
DepoSetBoundarySource ─▶ DepoSetDrifter ─▶ DepoTransform ─▶ FrameBoundarySink
                              │                   │
                           Drifter          AnodePlane ──── WireSchemaFile
                           Random           3× PlaneImpactResponse ──── FieldResponse
                                            FftwDFT                    ColdElecResponse
```

| PHLEX module | WCT sub-graph | Jsonnet config |
|---|---|---|
| `wcph_deposet_source_file` | `DepoFileSource → DepoSetBoundarySink` | `deposet-file-source.jsonnet` |
| `wcph_deposet_to_frame` | `DepoSetBoundarySource → DepoSetDrifter → DepoTransform → FrameBoundarySink` | `deposet-drift-sim.jsonnet` |
| `wcph_frame_sink_file` | `FrameBoundarySource → FrameFileSink` | `frame-file-sink.jsonnet` |

## New executor: FrameSinkFile (IFrame → file)

Mirrors `DepoSetSinkFile` exactly, with `IFrame` instead of `IDepoSet`.

The WCT sub-graph contains a `FrameBoundarySource` and a `FrameFileSink` (ITerminal).
Each `operator()(Frame)` call fills the boundary source and runs the WCT graph once.
`FrameFileSink` accumulates all frames internally; `WireCell::Main::~Main()` calls
`finalize()` to flush and close the output file.

**Important**: the `FrameSinkFile` destructor body is empty for the same reason as
`DepoSetSinkFile` — `Main::~Main()` already calls `finalize()`, so an explicit call
would double-finalize and crash with a `boost::iostreams::chain::pop()` assertion.

### WCT boundary node names

| Node type | Instance name | Derived from |
|---|---|---|
| `FrameBoundarySource` | `<module_label>_frame_source` | `m_scope` |
| `Pgrapher` | `<module_label>_pgrapher` | `m_scope` |

## Configuration reference

### `wcph_frame_sink_file`

| Key | Type | Default | Description |
|---|---|---|---|
| `wct_config` | string | required | Path to WCT Jsonnet (e.g. `frame-file-sink.jsonnet`) |
| `input_layer` | string | required | PHLEX layer of the input Frame product |
| `input_from` | string | required | Creator name of the upstream Frame product |
| `wct_plugins` | string[] | `[]` | Must include `WireCellPgraph`, `WireCellSio` |
| `wct_tla` | object | `{}` | Extra Jsonnet TLAs; use `{ outname: "path/to/frames.npz" }` |

### `wcph_deposet_to_frame` (with `deposet-drift-sim.jsonnet`)

| Key | Type | Default | Description |
|---|---|---|---|
| `wct_config` | string | required | `deposet-drift-sim.jsonnet` |
| `input_layer` | string | required | PHLEX layer of input DepoSet product |
| `wct_plugins` | string[] | `[]` | Must include `WireCellPgraph`, `WireCellGen`, `WireCellSigProc`, `WireCellAux` |

## Simulation physics configuration (PDSP APA 0)

`deposet-drift-sim.jsonnet` uses ProtoDUNE-SP APA 0 parameters for a realistic test:

```
Data files (via WIRECELL_PATH):
  protodune-wires-larsoft-v4.json.bz2  — wire geometry
  dune-garfield-1d565.json.bz2         — Garfield field response

Numerical parameters:
  tick           = 0.5 us
  nticks_ductor  = 10125   (10000 DAQ + 125 field response)
  readout_time   = 5.0625 ms
  start_time     = -62.5 us  (drift time to response plane)
  drift_speed    = 1.6 mm/us

Face geometry (APA 0, mm):
  Face 0 (front):  anode=-3578.36  response=-3487.8875  cathode=-1.5875
  Face 1 (back):   anode=-3683.14  response=-3773.6125  cathode=-7259.9125

Electronics: ColdElecResponse  gain=14 mV/fC  shaping=2.0 us
```

The same face geometry serves as both `AnodePlane` faces and `Drifter` xregions,
ensuring the drift simulation and plane mapping are consistent.

## FrameFileSink output format

`FrameFileSink` writes a WCT "frame file" `.npz` containing Numpy arrays:
- `frame_<tag>_<N>.npy` — 2-D float array of trace samples (channels × ticks)
- `channels_<tag>_<N>.npy` — 1-D int array of channel indices
- `tickinfo_<tag>_<N>.npy` — [tick0, dt, nticks]

The `sim-frames.npz` output (~22 KB) contains frames tagged with `""` (default),
one per input depo set (1 frame from `muon-depos.npz`).

## Required WCT plugins per module

| Module | Plugins |
|---|---|
| `wcph_deposet_source_file` | `WireCellPgraph`, `WireCellSio` |
| `wcph_deposet_to_frame` (drift+sim) | `WireCellPgraph`, `WireCellGen`, `WireCellSigProc`, `WireCellAux` |
| `wcph_frame_sink_file` | `WireCellPgraph`, `WireCellSio` |

## Jsonnet array concatenation note

Jsonnet uses `+` (not `++`) for array concatenation.  The `++` operator does not
exist in Jsonnet; writing `a ++ b` is parsed as `a + (+b)` (unary `+` on `b`), which
fails at runtime when `b` is an array.  Use `a + b` where both `a` and `b` are arrays.

## Adapting to other simulation configurations

To run a different set of WCT simulation components:

1. Write a Jsonnet config accepted by the `DepoSetToFrame` executor:
   ```jsonnet
   function(source_name="...", sink_name="...", app_name="...")
   [ {type:"DepoSetBoundarySource", name:source_name, ...},
     // ... simulation components ...
     {type:"FrameBoundarySink",     name:sink_name,   ...},
     {type:"Pgrapher", name:app_name, data:{edges:[...]}} ]
   ```
2. Use `wcph_deposet_to_frame` in the PHLEX workflow with `wct_config: "my-sim.jsonnet"`.
3. Pipe the output to `wcph_frame_sink_file` with `input_from: '<module_label>'`.

Replace `AnodePlane` faces, `FieldResponse` data file, and electronics parameters to
match your detector geometry.
