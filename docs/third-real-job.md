# Third Real Job: OmnibusSigProc (signal processing) in PHLEX

This document describes the integration of WCT's `OmnibusSigProc` signal
processing component into PHLEX workflows via wire-cell-phlex.  Two variants
are provided: a standalone frame-in/frame-out pipeline and a combined
sim+sigproc pipeline.

## Overview

`OmnibusSigProc` implements `IFrameFilter` (IFrame → IFrame) and lives in the
`WireCellSigProc` plugin.  It applies noise filtering, ROI finding, and signal
deconvolution to produce calibrated signal frames from raw (simulated) ADC frames.

## Variant A — standalone signal processing

Reads simulated frames from disk, applies signal processing, writes output frames.

```
┌──────────────────────┐    ┌──────────────────────────┐    ┌──────────────────────┐
│ wcph_frame_source_    │    │ wcph_frame_filter          │    │ wcph_frame_sink_file  │
│ file                 │───▶│ (OmnibusSigProc)          │───▶│ (FrameFileSink)      │
│ reads sim-frames.npz │    │                           │    │ writes sp-frames.npz │
└──────────────────────┘    └──────────────────────────┘    └──────────────────────┘
```

| PHLEX module | WCT sub-graph | Jsonnet config |
|---|---|---|
| `wcph_frame_source_file` | `FrameFileSource → FrameBoundarySink` | `frame-file-source.jsonnet` |
| `wcph_frame_filter` | `FrameBoundarySource → OmnibusSigProc → FrameBoundarySink` | `frame-sigproc.jsonnet` |
| `wcph_frame_sink_file` | `FrameBoundarySource → FrameFileSink` | `frame-file-sink.jsonnet` |

## Variant B — combined drift+sim+sigproc

Reads depo sets from disk, drifts + simulates + signal-processes in one executor.

```
┌──────────────────────┐    ┌─────────────────────────────────────────┐    ┌──────────────────────┐
│ wcph_deposet_source_  │    │ wcph_deposet_to_frame                    │    │ wcph_frame_sink_file  │
│ file                 │───▶│ (Drifter+DepoTransform+OmnibusSigProc)  │───▶│ (FrameFileSink)      │
│ reads muon-depos.npz │    │                                         │    │ writes sp-frames.npz │
└──────────────────────┘    └─────────────────────────────────────────┘    └──────────────────────┘
```

| PHLEX module | WCT sub-graph | Jsonnet config |
|---|---|---|
| `wcph_deposet_source_file` | `DepoFileSource → DepoSetBoundarySink` | `deposet-file-source.jsonnet` |
| `wcph_deposet_to_frame` | `DepoSetBoundarySource → DepoSetDrifter → DepoTransform → OmnibusSigProc → FrameBoundarySink` | `deposet-sim-sigproc.jsonnet` |
| `wcph_frame_sink_file` | `FrameBoundarySource → FrameFileSink` | `frame-file-sink.jsonnet` |

## New executor: FrameSourceFile (file → IFrame)

Mirrors `DepoSetSourceFile` exactly, with `IFrame` instead of `IDepoSet`.

The WCT sub-graph contains a `FrameFileSource` (WireCellSio) and a
`FrameBoundarySink`.  On the first `operator()()` call the entire WCT graph
runs to completion, queuing all frames in the sink buffer.  Subsequent calls
drain one `Frame` per call.

**Important**: the PHLEX workflow `total` must match the number of frames in
the input file.

### WCT boundary node names

| Node type | Instance name | Derived from |
|---|---|---|
| `FrameBoundarySink` | `<module_label>_frame_sink` | `m_scope` |
| `Pgrapher` | `<module_label>_pgrapher` | `m_scope` |

## OmnibusSigProc dependencies

`OmnibusSigProc` shares several components with `DepoTransform`:

| Component | Used by | Notes |
|---|---|---|
| `AnodePlane` | both | PDSP APA 0 geometry |
| `FftwDFT` | both | Same instance |
| `FieldResponse` | both | Same `dune-garfield-1d565.json.bz2` |
| `ColdElecResponse` | DepoTransform (`short_responses`), OmnibusSigProc (`elecresponse`) | Same instance |
| 13 SP filter objects | OmnibusSigProc only | Defined in `sp-filters.jsonnet` |

In Variant B's combined config (`deposet-sim-sigproc.jsonnet`), all of these
are instantiated once and shared.

## SP filter objects (sp-filters.jsonnet)

`OmnibusSigProc` looks up 13 filter components by hard-coded instance name.
The names must not be changed.  `cfg/sp-filters.jsonnet` returns the array of
all 13 objects with PDSP-tuned parameters:

| Type | Instance names | PDSP parameters |
|---|---|---|
| `LfFilter` (3×) | `ROI_tight_lf`, `ROI_tighter_lf`, `ROI_loose_lf` | tau = 0.014, 0.06, 0.002 MHz |
| `HfFilter` Gaussian (2×) | `Gaus_tight`, `Gaus_wide` | sigma = 0, 0.12 MHz |
| `HfFilter` Wiener (6×) | `Wiener_tight_{U,V,W}`, `Wiener_wide_{U,V,W}` | PDSP-tuned sigma+power |
| `HfFilter` wire (2×) | `Wire_ind`, `Wire_col` | flag=false, dimensionless sigma |

## OmnibusSigProc PDSP parameters

```jsonnet
{
    anode:          wc.tn(anode),
    dft:            wc.tn(dft),
    field_response: wc.tn(fr),
    elecresponse:   wc.tn(elec),   // ColdElecResponse (IWaveform)
    per_chan_resp:   "",             // disabled (no PDSP chresp file)
    ftoffset:       0.0,
    ctoffset:       1.0 * wc.us,
    postgain:       1.0,
    ADC_mV:         4095.0 / (1.4 * wc.volt),  // 12-bit, 1.4V full-scale
    fft_flag:       0,
    sparse:         true,           // output sparse (non-zero traces only)
    // ROI thresholds (PDSP defaults)...
}
```

Note: `elecresponse` expects an `IWaveform` type-name (not an array).
`ColdElecResponse` satisfies both `IWaveform` (used by OmnibusSigProc) and
`IWaveform[]` (used by PlaneImpactResponse `short_responses`).

## Required WCT plugins per module

| Workflow | Module | Plugins |
|---|---|---|
| Variant A | `wcph_frame_source_file` | `WireCellPgraph`, `WireCellSio` |
| Variant A | `wcph_frame_filter` (sigproc) | `WireCellPgraph`, `WireCellGen`, `WireCellSigProc`, `WireCellAux` |
| Variant A | `wcph_frame_sink_file` | `WireCellPgraph`, `WireCellSio` |
| Variant B | `wcph_deposet_source_file` | `WireCellPgraph`, `WireCellSio` |
| Variant B | `wcph_deposet_to_frame` (sim+sigproc) | `WireCellPgraph`, `WireCellGen`, `WireCellSigProc`, `WireCellAux` |
| Variant B | `wcph_frame_sink_file` | `WireCellPgraph`, `WireCellSio` |

Note: `WireSchemaFile` is in `WireCellGen`, not `WireCellAux`.  Any executor
that uses `WireSchemaFile` must include `WireCellGen` in `wct_plugins`.

## Configuration reference

### `wcph_frame_source_file`

| Key | Type | Default | Description |
|---|---|---|---|
| `wct_config` | string | required | Path to WCT Jsonnet (e.g. `frame-file-source.jsonnet`) |
| `output_layer` | string | required | PHLEX layer for output Frame product |
| `wct_plugins` | string[] | `[]` | Must include `WireCellPgraph`, `WireCellSio` |
| `wct_tla` | object | `{}` | Use `{ inname: "path/to/frames.npz" }` |

## Test dependency note

The `phlex_frame_sigproc` test (Variant A) reads `sim-frames.npz` produced by
`phlex_deposet_sim`.  A ctest fixture dependency ensures the correct ordering:
`phlex_deposet_sim` is declared with `FIXTURES_SETUP phlex_deposet_sim_fixture`
and `phlex_frame_sigproc` with `FIXTURES_REQUIRED phlex_deposet_sim_fixture`.
