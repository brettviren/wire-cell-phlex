# DUNE WCT Configuration Layer

This document describes the factored WCT Jsonnet configuration system under
`cfg/dune/`, how it relates to the older flat configs in `cfg/`, and how to
extend it to new detectors or variants.

---

## Motivation

The flat configs in `cfg/` (e.g. `deposet-drift-sim.jsonnet`,
`frame-sigproc.jsonnet`, `pdhd-apa-sim-sigproc.jsonnet`) hardcode a single
detector's parameters and work well for their specific test cases.  They are
not suitable as a base for supporting multiple detectors because detector
constants are scattered throughout rather than centralized.

The `cfg/dune/` layer solves this by separating concerns:

- **What the detector is** (geometry, electronics, response files, digitizer
  settings) lives in `wct/dets/`.
- **How to build a WCT subgraph** from a detector description lives in
  `wct/job/`.
- **How to assemble a PHLEX workflow** from a job function lives in
  `phlex/job/`.

The flat `cfg/` files and their tests are unchanged; `cfg/dune/` is additive.

---

## Directory layout

```
cfg/dune/
  wct/
    dets/
      pdhd/
        detector.jsonnet       -- PDHD detector description function
        sp-filters.jsonnet     -- PDHD SP filter array (16 entries)
      pdvd/
        detector.jsonnet       -- PDVD detector description function
        sp-filters.jsonnet     -- PDVD SP filter array (13 entries)
    dets.jsonnet               -- dynamic import map {pdhd: fn, pdvd: fn}
    job/
      sim.jsonnet              -- drift + electronics simulation subgraph
      sigproc.jsonnet          -- OmnibusSigProc signal processing subgraph
      sim-sigproc.jsonnet      -- combined sim + sigproc subgraph
  phlex/
    job/
      sim-sigproc.jsonnet      -- PHLEX workflow builder function
```

---

## The detector description object

### Function signature

Each `dets/*/detector.jsonnet` is a Jsonnet function with the same signature:

```jsonnet
function(params={detname: "pdhd"})
```

`params` is an override object.  `params.detname` (the canonical detector
name) is the only required field.  Optional fields are deep-merged into the
defaults:

- `params.lar` — override LAr bulk properties (`DL`, `DT`, `lifetime`,
  `drift_speed`)
- `params.daq` — override DAQ parameters (`tick`, `nticks`)
- `params.sim` — override simulation flags (`fluctuate`, `tick0_time`, ...)
- `params.elec_gain` — override FE amplifier gain (PDHD only; selects noise
  file automatically)

Example:
```jsonnet
local det = dets["pdhd"]({ detname: "pdhd", lar: { lifetime: 35.0 * wc.ms } });
```

### Return schema

The function returns a plain-data object (no WCT `{type, name, data}` component
configs, except `sp_filters` which must be WCT components because OmnibusSigProc
looks up filter instances by hard-coded names at runtime).

Top-level fields:

| Field | Type | Description |
|---|---|---|
| `name` | string | Canonical detector name (`"pdhd"` or `"pdvd"`) |
| `daq.tick` | number | Sample period (WCT internal units, e.g. `0.5*wc.us`) |
| `daq.nticks` | int | Readout ticks per event |
| `lar.DL` / `lar.DT` | number | Diffusion constants |
| `lar.lifetime` | number | Electron lifetime |
| `lar.drift_speed` | number | Nominal drift velocity |
| `sim.fluctuate` | bool | Enable charge fluctuations in simulation |
| `sim.tick0_time` | number | Absolute G4 time of readout tick 0 |
| `sim.nsigma` | int | Depo spread cutoff in DepoTransform |
| `sim.nimpacts` | int | Impact positions per wire pitch in AnodePlane |
| `response_plane` | number | Distance from collection wires to response plane start; **must match the Garfield field response files** |
| `wires.filename` | string | Wire geometry data file |
| `anodes` | array | Per-anode entries (see below) |
| `sp_filters` | array | WCT filter component configs (LfFilter/HfFilter) |
| `sys_status` | bool | Enable response systematics |
| `sys_resp` | object | Systematic response parameters |
| `rc_resp` | object | RC response parameters |
| `bounds` | object | Overall detector bounding box (informational) |

### Per-anode fields (`det.anodes[i]`)

The `anodes` array is the primary axis of per-readout-unit variation.  All
fields that can differ from one anode to another are here, avoiding
conditional logic in job functions.

| Field | Type | Description |
|---|---|---|
| `ident` | int | AnodePlane identifier (index into WireSchema) |
| `name` | string | Human-readable name (`"apa0"`, `"anode4"`, ...) |
| `faces` | array | Drift region geometry: `[{anode, response, cathode}, ...]` |
| `elec.type` | string | `"ColdElecResponse"` or `"JsonElecResponse"` |
| `elec.gain` | number | FE amplifier gain (ColdElecResponse only) |
| `elec.shaping` | number | Shaping time (ColdElecResponse only) |
| `elec.postgain` | number | Post-amplifier gain factor |
| `elec.filename` | string | Response file path (JsonElecResponse only) |
| `field.filename` | string | Garfield field response data file |
| `filter_response` | object or null | `{filename}` for SP filter correction file, or `null` |
| `noise.filename` | string | Empirical noise spectra file |
| `noise.wire_length_scale` | number | Wire length scale for noise model |
| `adc.resolution` | int | ADC bit depth |
| `adc.gain` | number | ADC input gain |
| `adc.baselines` | array | Per-plane ADC baseline (U, V, W) |
| `adc.fullscale` | array | ADC voltage range `[min, max]` |
| `sigproc.ctoffset` | number | Collection time offset for OmnibusSigProc |
| `sigproc.r_th_factor` | number | ROI threshold factor |
| `sigproc.plane2layer` | array | Wire plane → deconvolution layer mapping |
| `sigproc.wiener_filters` | array | Wiener filter instance names `[U, V, W]` |
| ... | ... | Full OmnibusSigProc tuning parameters |

### Face geometry convention

Faces are always provided in **sim-mode** (both faces active, no `null` entries).
Data-mode jobs that need a single-sided anode can filter the `faces` array:

```jsonnet
// Use only face 0 (data mode)
local single_face_anode = anode { faces: [anode.faces[0], null] };
```

The sim-mode default is chosen because it is the superset: a sim job needs
both faces to correctly simulate deposits that enter from the cryo wall side.

---

## The `dets.jsonnet` dynamic import

```jsonnet
// cfg/dune/wct/dets.jsonnet
{
    pdhd: import "dets/pdhd/detector.jsonnet",
    pdvd: import "dets/pdvd/detector.jsonnet",
}
```

Each value is a function (not yet called).  Callers select by name and call
with their params object:

```jsonnet
local dets = import "dune/wct/dets.jsonnet";
local det  = dets[params.detname](params);
```

Because Jsonnet imports are lazy, both detector files are only parsed when
accessed.  Adding a new detector requires one new entry here.

---

## WCT job functions (`wct/job/`)

Each file is a Jsonnet function parameterized by TLA strings (because the
PHLEX executor passes all TLAs as strings).  They import `dune/wct/dets.jsonnet`
internally to obtain the detector description.

Common TLA parameters:

| TLA | Default | Description |
|---|---|---|
| `source_name` | `"wcphlex_*_source"` | BoundarySource instance name (injected by Executor) |
| `sink_name` | `"wcphlex_*_sink"` | BoundarySink instance name (injected by Executor) |
| `app_name` | `"wcphlex_pgrapher"` | Pgrapher instance name (injected by Executor) |
| `detector` | `"pdhd"` | Canonical detector name |
| `anode_index` | `"0"` | Index into `det.anodes[]` (string, parsed internally) |
| `service_prefix` | `""` | WCT service name prefix (see `docs/services.md`) |

### Derivations performed in job functions

Job functions compute the following from the detector object rather than
requiring them in the descriptor:

- `response_nticks = roundToInt(det.response_plane / det.lar.drift_speed / det.daq.tick)`
- `nticks_ductor = det.daq.nticks + response_nticks`
- `start_time = det.sim.tick0_time - det.response_plane / det.lar.drift_speed`
- `ADC_mV = ((1 << adc.resolution) - 1) / (fullscale[1] - fullscale[0])`

Electronics response construction dispatches on `anode.elec.type`:

```jsonnet
local elec_data = if a.elec.type == "ColdElecResponse" then {
    shaping:  a.elec.shaping,
    gain:     a.elec.gain,
    postgain: a.elec.postgain,
} else if a.elec.type == "JsonElecResponse" then {
    filename: a.elec.filename,
    postgain: a.elec.postgain,
} else error "Unknown elec type: " + a.elec.type;
```

Optional `FilterResponse` components (for SP deconvolution correction) are
generated only when `anode.filter_response != null`.

---

## PHLEX job functions (`phlex/job/`)

### `phlex/job/sim-sigproc.jsonnet`

A Jsonnet function that returns a complete PHLEX workflow object:

```jsonnet
function(params, depo_file="muon-depos.npz", output_file="frames.npz",
         anode_index=0, nevents=1)
```

`params.detname` selects the detector.  Returns the driver/sources/modules
structure consumed by the `phlex` CLI.

### The "params trick" for detector selection

The `phlex` CLI (as of version 0.2.0) has no command-line TLA support.  A
main workflow Jsonnet outside `cfg/dune/` uses a file import as a runtime
selector:

```jsonnet
// my-pdhd-workflow.jsonnet  (on WIRECELL_PATH)
local params      = import "wire-cell-phlex-detector-params.jsonnet";
local sim_sigproc = import "dune/phlex/job/sim-sigproc.jsonnet";
sim_sigproc(params, depo_file="my-depos.npz", output_file="out.npz")
```

The user places a detector params file on `WIRECELL_PATH` with the canonical
name `wire-cell-phlex-detector-params.jsonnet`:

```jsonnet
// pdhd-sim-params.jsonnet  →  symlink or copy as wire-cell-phlex-detector-params.jsonnet
{ detname: "pdhd", lar: { lifetime: 35.0 * wc.ms } }
```

`WIRECELL_PATH` is searched left-to-right, so a local directory with the
params file shadows any other location.

---

## SP filter objects

`OmnibusSigProc` looks up 13–16 `LfFilter`/`HfFilter` instances by names
hard-coded in C++.  These are placed in `det.sp_filters` as WCT component
configs and included verbatim in every job's component list.

PDHD and PDVD use different filter tuning values and PDHD adds three extra
`Wiener_tight_*_APA1` filters (16 total vs 13) used for APA0 which has a
higher-quality field response.  The per-anode `sigproc.wiener_filters` array
records which Wiener filter names apply to each anode:

```jsonnet
// pdhd/detector.jsonnet
sigproc: {
    wiener_filters: if n == 0
        then ["Wiener_tight_U_APA1", "Wiener_tight_V_APA1", "Wiener_tight_W_APA1"]
        else ["Wiener_tight_U",      "Wiener_tight_V",      "Wiener_tight_W"],
    // ...
},
```

All filter instances are always registered in the WCT factory regardless of
which anode is active, since the factory is global and all filter names must
be resolvable.

---

## Tests

### Jsonnet evaluation tests

`test/compare_wct_configs.py` compares two WCT configuration JSON arrays
(produced by `jsonnet`) component-by-component:
- Matches by `(type, name)` key
- Reports components present in one file only (expected: wcls art-adapter
  nodes in dunereco, boundary nodes in phlex)
- Recursively diffs `data` fields with configurable float tolerance
- `--skip-type` and `--skip-name` options for known structural differences

Reference test matrix (to be run manually; not yet in ctest):

| dunereco config | phlex config | Notes |
|---|---|---|
| `pdhd/wct-sim-check.jsonnet` | `dune/wct/job/sim.jsonnet` (pdhd, 0) | Service component values should match |
| `pdhd/wcls-nf-sp.jsonnet` | `dune/wct/job/sigproc.jsonnet` (pdhd, 0) | SP stage only; NF nodes excluded |
| `protodunevd/wct-sim-check.jsonnet` | `dune/wct/job/sim.jsonnet` (pdvd, 0) | PDVD bottom drift |

### PHLEX integration test

`phlex_pdhd_sim_sigproc_factored` runs the full PDHD APA0 pipeline via the
new factored configs and compares the output frames NPZ against the existing
`phlex_sim_sigproc` reference (tolerance: ≤2 ADC, ≤1 channel difference).

---

## Limitations and known issues

### `response_plane` is detector-level, not anode-level

`det.response_plane` is a single value shared by all anodes of a detector.
This is valid for PDHD (all APAs use the same Garfield calculations) and PDVD
(all anodes use the same field response file).  If a future detector uses
different field response files calculated at different starting distances for
different anodes, `response_plane` would need to move into the per-anode entry.

The necessary change is localized: move `response_plane` into each anode object
and change the three job functions to read `a.response_plane` instead of
`det.response_plane`.

### `sim.tick0_time` is a single origin for all anodes

DepoTransform's `start_time` is derived from `det.sim.tick0_time`.  For
detectors where different anodes have different clock origins (e.g. multi-drift
TPC modules with independent timing), `tick0_time` would need to be per-anode.
Migration path: add an optional `anode.tick0_time` field and fall back to
`det.sim.tick0_time` in job functions.

### Single shared drifter per island

`wct/job/sim.jsonnet` creates a `Drifter` that uses only the faces of the
selected anode as its `xregions`.  Deposits in other anodes' drift regions
are silently discarded.  This is correct for the single-APA-per-island pattern
used in wire-cell-phlex.

For a "global drifter" that drifts all deposits before fan-out (as in
dunereco's `wcls-sim-drift-*.jsonnet`), the caller must build the Drifter
separately using all faces from `det.anodes[*].faces`.  A helper job function
for this pattern (`wct/job/drifter.jsonnet`) is not yet written.

### Face geometry is always sim-mode (both faces active)

`detector.jsonnet` always returns both faces active per anode.  For data-mode
processing (where one face per APA is null because the cryostat wall blocks
that drift volume), jobs must filter the `faces` array themselves.  No
data-mode helper is provided yet.

### SP filter objects are always in the component list

`det.sp_filters` includes all filter instances for that detector regardless of
which anode is being processed.  For PDHD this means the three
`Wiener_tight_*_APA1` components are always registered even when processing
APA1–3 (which don't use them).  This is harmless — unused components are
registered in the WCT factory but never invoked — but it does mean PDHD and
PDVD cannot share a `sp_filters` set in the same process without one set's
names overwriting the other's (since the factory is global and both sets use
the same names for the 13 common filters).

If PDHD and PDVD anodes were ever processed in the same `phlex` invocation,
the `service_prefix` mechanism (`docs/services.md`) would need to be extended
to cover SP filter names, or the SP filter names would need to be made
detector-specific.  At present this situation does not arise.

### No NF (noise filtering) or DNN ROI support

NF components (`PDHDOneChannelNoise`, `PDHDCoherentNoiseSub`,
`PDVDShieldCouplingSub`, etc.) require detector-specific channel group arrays
that are large, hardcoded, and not easily abstracted.  They are deferred to a
future phase.

Similarly, DNN ROI finding (`UNet`-based) is detector- and model-specific.

---

## Adding a new detector

### Step 1: Create `cfg/dune/wct/dets/<name>/`

- Write `detector.jsonnet` following the same schema.  Every field in the
  schema must be present; `null` is valid for optional fields like
  `filter_response`.
- Write `sp-filters.jsonnet` returning the detector's tuned filter array.
  If the new detector reuses an existing set exactly, it can import it:
  ```jsonnet
  // cfg/dune/wct/dets/newdet/sp-filters.jsonnet
  import "../pdvd/sp-filters.jsonnet"
  ```

### Step 2: Register in `dets.jsonnet`

```jsonnet
{
    pdhd:   import "dets/pdhd/detector.jsonnet",
    pdvd:   import "dets/pdvd/detector.jsonnet",
    newdet: import "dets/newdet/detector.jsonnet",
}
```

### Step 3: Validate

```bash
jsonnet -J cfg -J <wct-share-dir> \
    --tla-code 'params={detname:"newdet"}' \
    cfg/dune/wct/dets/newdet/detector.jsonnet
```

Then spot-check a job:

```bash
jsonnet -J cfg -J <wct-share-dir> \
    --tla-str detector=newdet --tla-str anode_index=0 \
    cfg/dune/wct/job/sim.jsonnet
```

### Step 4: Add a PHLEX test workflow

Copy `test/pdhd-sim-sigproc-workflow.jsonnet.in`, set `detname` and file paths,
add a `configure_file` and `add_test` entry to `CMakeLists.txt`.

---

## Adding a new job type

A new job (e.g. simulation-only without sigproc, or NF-only) follows the
same pattern as the existing `wct/job/` files:

1. Write `cfg/dune/wct/job/<jobname>.jsonnet` as a TLA function.  Import
   `dune/wct/dets.jsonnet`, call `dets[detector](params)`, iterate over
   `det.anodes[ai]`.

2. If the job needs PHLEX-level orchestration, write
   `cfg/dune/phlex/job/<jobname>.jsonnet` as a `function(params, ...)` that
   builds the workflow object.

3. The job function must handle any schema fields that may be `null` or absent.
   Guard with `std.get(obj, "field", default)` or `if field != null then ...`.

---

## Relationship to dunereco configs

The `cfg/dune/` layer was designed by studying the dunereco
`DUNEWireCell/{pdhd,protodunevd}/` configurations.  Key translation decisions:

| dunereco pattern | wire-cell-phlex pattern |
|---|---|
| `std.extVar("elecGain")` | `params.elec_gain` TLA default (14.0) |
| `std.extVar("Nbit")` | hardcoded 14 in `adc.resolution` |
| `std.extVar("reality")` | always sim-mode; data-mode is a job concern |
| `wcls_input.adc_digits` | `FrameBoundarySource` |
| `wcls_output.sp_signals` | `FrameBoundarySink` |
| `pgrapher/common/tools.jsonnet` | `wct/job/*.jsonnet` functions |
| `pdhd/params.jsonnet` inheritance chain | flat `detector.jsonnet` with `+` merge |
| Per-anode `if ident < 4` conditionals | per-entry array with values encoded per-anode |

The two codebases will diverge over time as wire-cell-phlex adds detectors
and job types.  The dunereco configs remain the authoritative reference for
parameter values; `test/compare_wct_configs.py` is the tool for checking that
the two produce equivalent WCT component configurations.
