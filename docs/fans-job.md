# Fans Job: 4-APA PDHD Fan-Out / Fan-In in PHLEX

This document describes the implementation of a PDHD 4-APA fan-out / fan-in
pipeline in PHLEX via wire-cell-phlex.  Real detectors have multiple Anode
Plane Assemblies (APAs): PDHD has 4 (a 2×2 grid), FDHD has 200 (25×4×2).
This push exercises both PHLEX-level fan capabilities and the new
`FrameFaninSinkFile` executor.

## Pipeline topology

```
                      ┌── wcp_deposet_to_frame (sim_0, APA 0) ──┐
                      │   DepoTransform (PDHD APA 0 geometry)   │
┌──────────────────┐  ├── wcp_deposet_to_frame (sim_1, APA 1) ──┤  ┌──────────────────────────┐
│ wcp_deposet_     │  │   DepoTransform (PDHD APA 1 geometry)   │  │ wcp_frame_fanin_sink_file│
│ source_file      │──├── wcp_deposet_to_frame (sim_2, APA 2) ──┼──│ 4×FrameBoundarySource    │
│ DepoFileSource + │  │   DepoTransform (PDHD APA 2 geometry)   │  │ → FrameFanin(mult=4)     │
│ Drifter          │  └── wcp_deposet_to_frame (sim_3, APA 3) ──┘  │ → FrameFileSink          │
│ (all 4 APAs)     │                                                └──────────────────────────┘
└──────────────────┘
  PHLEX fan-out                                             PHLEX + WCT fan-in
 (TBB broadcast)                                       (join_node<4> + FrameFanin)
```

## Fan-out mechanism (PHLEX-level)

The source produces a `DepoSet` with `creator="input"`.  All four
`wcp_deposet_to_frame` instances have no explicit `input_from` override
(they all consume from `creator="input"`).  PHLEX's `edge_maker` creates four
TBB edges from the single source output port; TBB broadcasts the same message
to all four consumers automatically.

## Fan-in mechanism (PHLEX-level + WCT-level)

**PHLEX fan-in**: The `wcp_frame_fanin_sink_file` module uses
`input_family(q0, q1, q2, q3)` with four `product_query` arguments.  PHLEX
creates a `multilayer_join_node<4>` (TBB `join_node` with tag-matching) that
fires only when all four Frames for the same event have arrived, regardless of
which order the four `wcp_deposet_to_frame` instances complete.

**WCT fan-in**: Inside `wcp_frame_fanin_sink_file`, the WCT sub-graph
`frame-fanin-file-sink.jsonnet` has four `FrameBoundarySource` nodes (one per
APA), each feeding one input port of `FrameFanin(multiplicity=4)`.  WCT's
`FrameFanin` performs the non-trivial merge: concatenates traces from all four
input Frames, merges channel masks, takes ident/time/tick from the first
input.  The merged Frame is then written to disk by `FrameFileSink`.

## PDHD geometry

Wire schema: `protodunehd-wires-larsoft-v1.json.bz2`.
Field response: `dune-garfield-1d565.json.bz2` (generic; PDHD-specific response
not available in the local Spack view).

From `wire-cell-toolkit/cfg/pgrapher/experiment/pdhd/simparams.jsonnet`:

| Constant | Value | Notes |
|---|---|---|
| `apa_cpa` | 3.5734 m | APA-to-CPA distance / APA centerline magnitude |
| `apa_plane` | 52.455 mm | APA centerline to outermost wire plane |
| `res_plane` | 142.935 mm | APA centerline to response plane |
| `cpa_plane` | 3571.8125 mm | APA centerline to cathode |
| nticks | 6000 | at tick = 0.5 µs → 3 ms readout |

APA n has centerline at `sign × apa_cpa` where `sign = 2×(n%2) − 1`:

| APA | sign | cl (m) | Drift column |
|---|---|---|---|
| 0 | −1 | −3.5734 | left |
| 1 | +1 | +3.5734 | right |
| 2 | −1 | −3.5734 | left (same x as APA 0) |
| 3 | +1 | +3.5734 | right (same x as APA 1) |

APAs 0/2 share the same x-position (left column); APAs 1/3 share the right
column.  The wire schema distinguishes them by APA `ident` (0–3), which maps
to disjoint channel ranges.

The Drifter in `pdhd-file-drifter.jsonnet` uses 4 xregions (both faces of
each drift column) so depos drifting toward either side are captured.

## WCT component naming (per-APA uniqueness)

Each of the four `DepoSetToFrame` executor instances loads a separate
`WireCell::Main` that contributes new components to the WCT global factory.
To prevent collisions, all per-APA component names include the APA identifier:

| Component | Name pattern | Example |
|---|---|---|
| FftwDFT | `dft_apaN` | `dft_apa0` |
| WireSchemaFile | `wires_apaN` | `wires_apa1` |
| FieldResponse | `fr_apaN` | `fr_apa2` |
| ColdElecResponse | `elec_apaN` | `elec_apa3` |
| AnodePlane | `apaN` | `apa0` |
| PlaneImpactResponse | `pir{U,V,W}_apaN` | `pir0_apa1` |
| DepoTransform | `transform_apaN` | `transform_apa2` |

The executor scope (module label) contributes the TLA `source_name`,
`sink_name`, `app_name` for the boundary nodes and Pgrapher.

## OmnibusSigProc: concurrent SP filter limitation

`OmnibusSigProc` looks up 13 SP filter objects by **hard-coded names** (e.g.
`ROI_tight_lf`, `Gaus_tight`) in the WCT global factory.  When multiple
`OmnibusSigProc` instances are initialized (from multiple `Main` instances),
the WCT factory returns the **same shared pointer** for these objects.

When four TBB worker threads call those filter objects' `operator()(IFrame)`
concurrently, there is a data race on their internal Eigen arrays — a data
race that manifests as an Eigen bounds assertion failure.  Three or fewer
concurrent instances work by coincidence (TBB may serialize them), but four
reliably crash.

**Consequence**: The fans test uses `pdhd-apa-sim.jsonnet` (DepoTransform
only, no OmnibusSigProc) rather than `pdhd-apa-sim-sigproc.jsonnet`.  This
still exercises the complete PHLEX fan topology.  Adding sigproc to the fans
pipeline requires either:

1. Making the SP filter objects thread-safe in WCT (future WCT improvement).
2. Running OmnibusSigProc as a separate `wcp_frame_filter` pass after the
   per-APA sim, inside a single executor that processes all APAs sequentially.
3. Building a monolithic single-`Main` executor for the whole 4-APA pipeline.

The `pdhd-apa-sim-sigproc.jsonnet` config is retained for single-APA use (e.g.
in future per-APA PHLEX workflows with sequential execution) and as a
template for when WCT makes SP filters thread-safe.

## Test data note

`muon-depos.npz` was generated with PDSP geometry.  PDSP depos span x ∈ [−7260,
−1.6] mm.  PDHD face 0 of APA 0 spans x ∈ [−3521, −1.6] mm, so a subset of
PDSP depos falls within this PDHD drift volume.  Some faces in the right column
(APAs 1 and 3 at x ≈ +3573 mm) receive no depos at all, producing empty Frames.
`FrameFanin` merges empty Frames correctly; the merged output file is valid but
physics-empty.  The test validates topology and data flow only.

## New files

| File | Description |
|---|---|
| `wire_cell_phlex/Executor.h` | Added `FrameFaninSinkFile` class |
| `wire_cell_phlex/Executor.cpp` | Added `FrameFaninSinkFile` implementation |
| `modules/frame_fanin_sink_file.cpp` | PHLEX 4-input observer module |
| `cfg/frame-fanin-file-sink.jsonnet` | WCT: 4×FrameBoundarySource → FrameFanin → FrameFileSink |
| `cfg/pdhd-file-drifter.jsonnet` | WCT: DepoFileSource + Drifter (whole-detector xregions) |
| `cfg/pdhd-apa-sim.jsonnet` | WCT: DepoTransform per APA (no sigproc) |
| `cfg/pdhd-apa-sim-sigproc.jsonnet` | WCT: DepoTransform + OmnibusSigProc per APA (single-APA use) |
| `test/fans-workflow.jsonnet.in` | PHLEX test workflow (CMake template) |

## PHLEX module reference

| Module | Description |
|---|---|
| `wcp_frame_fanin_sink_file` | 4-input observer: receives 4 Frames, fills 4 FrameBoundarySources, runs FrameFanin → FrameFileSink |

### `wcp_frame_fanin_sink_file` config keys

| Key | Type | Default | Description |
|---|---|---|---|
| `wct_config` | string | required | Path to `frame-fanin-file-sink.jsonnet` |
| `input_layer` | string | required | PHLEX layer for all 4 Frame products |
| `input_from_0` | string | required | creator of APA 0 Frame |
| `input_from_1` | string | required | creator of APA 1 Frame |
| `input_from_2` | string | required | creator of APA 2 Frame |
| `input_from_3` | string | required | creator of APA 3 Frame |
| `wct_plugins` | string[] | `[]` | Must include `WireCellPgraph`, `WireCellGen`, `WireCellSio` |
| `wct_tla` | object | `{}` | Use `{ outname: "path/to/output.npz" }` |

## Running the test manually

```sh
phlex -c build/fans-workflow.jsonnet   # generated by CMake
```
