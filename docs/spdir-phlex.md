# spdir-phlex: SPDIR metrics workflow for wire-cell-phlex

`test/scripts/spdir-phlex` is a Snakemake workflow that measures signal
processing quality as a function of track angle for a given DUNE detector.
It produces three summary metrics — efficiency, bias, and resolution — for
each wire plane, using the wire-cell-phlex framework and the factored
`cfg/dune/` configurations.

It is a translation of `wire-cell-toolkit/test/scripts/spdir` to PHLEX, with
these simplifications: single nominal field response only (no hi/lo variants),
no UVCGAN, detectors limited to those in `cfg/dune/wct/dets/`.

---

## Quick start

```bash
# Full angle set, pdhd (runs ~14 angles × 3 planes = 87 jobs)
test/scripts/spdir-phlex --config detector=pdhd --directory=/tmp/pdhd-spdir

# Quick test with 3 angles
test/scripts/spdir-phlex --directory=/tmp/pdhd-quick \
  --config detector=pdhd "theta_xz_deg=[10,45,80]" "theta_y_deg=[90,90,90]"

# pdvd
test/scripts/spdir-phlex --config detector=pdvd --directory=/tmp/pdvd-spdir
```

> **Snakemake 9 config note:** Snakemake 9+ replaces the entire config dict
> on each `--config` flag.  All key=value overrides must be in a single
> `--config` invocation (space-separated), not spread across multiple flags.

### Output files

All outputs are written to `--directory` (default: current directory).
The naming scheme is:

```
{detector}-{tier}-p{plane}-txz{txz}deg-ty{ty}deg.{ext}
```

| Tier | Ext | Description |
|------|-----|-------------|
| `depos` | `.npz` / `.json` | Track depositions and metadata |
| `digits` | `.npz` | Digitized ADC frames |
| `splats` | `.npz` | Truth-signal frames (DepoFluxSplat) |
| `signals` | `.npz` | Signal-processed frames |
| `metrics` | `.json` | Per-plane efficiency / bias / resolution |
| `mplots` | `.pdf` | Detailed per-angle splat-vs-signal comparison |
| `plots` | `.pdf` | Summary plots across all angles (wire or global coords) |

Summary plots are named without angle wildcards:

```
{detector}-plots-{coords}coords.pdf   # coords = wire | global
```

Intermediate `.workflow.json` files (one per `phlex` call) are left in the
output directory for debugging.

### Partial targets

Run through any tier by naming an aggregate rule:

```bash
# Stop after tracking
test/scripts/spdir-phlex --config detector=pdhd --directory=/tmp/out all_depos

# Stop after simulation
test/scripts/spdir-phlex --config detector=pdhd --directory=/tmp/out all_digits

# Stop after signal processing
test/scripts/spdir-phlex --config detector=pdhd --directory=/tmp/out all_signals
```

Available aggregate targets: `all_depos`, `all_digits`, `all_splats`,
`all_signals`, `all_metrics`, `all_mplots`, `all_plots`.

---

## Configuration reference

All keys are set via `--config key=value key2=value2 ...` in one flag.

| Key | Default | Description |
|-----|---------|-------------|
| `detector` | `pdhd` | Canonical detector name (must match `cfg/dune/wct/dets/`) |
| `apa` | `0` | APA (anode) index within the detector |
| `theta_xz_deg` | `[0,1,3,5,10,20,30,45,60,75,80,82,84,89]` | Track angles in X-Z plane (degrees) |
| `theta_y_deg` | `[90,90,...,90]` (14 entries) | Track angle from Y axis (degrees); must be same length as `theta_xz_deg` |
| `planes` | `[0,1,2]` | Wire plane indices to process |
| `phlex` | env `PHLEX` or `phlex` | Path to `phlex` executable |
| `wcpy` | env `WCPY` or `wcpy` | Path to `wcpy` executable |
| `wcsonnet` | env `WCSONNET` or `wcsonnet` | Path to `wcsonnet` executable |
| `wirecell_path` | auto-detected | Overrides `WIRECELL_PATH` |
| `phlex_plugin_path` | auto-detected | Overrides `PHLEX_PLUGIN_PATH` |

### Environment variables

When the config keys above are absent, environment variables are used:

| Variable | Purpose |
|----------|---------|
| `PHLEX` | Path to `phlex` executable |
| `WCPY` | Path to `wcpy` executable |
| `WCSONNET` | Path to `wcsonnet` executable |
| `WIRECELL_PATH` | Colon-separated dirs for WCT Jsonnet and data files |
| `PHLEX_PLUGIN_PATH` | Colon-separated dirs for `wcp_*.so` plugin libraries |

If none of these are set, the script auto-detects paths (see *Environment
auto-detection* below).

---

## Pipeline internals

Each `(detector, plane, theta_xz, theta_y)` combination runs through seven
steps.  Steps 2–4 run in parallel once their inputs are ready.

```
tracking ──► sim ──► sigproc ──► metrics ──► plots (all angles)
         └──► splat ──────────────────────► mplots (per-angle)
```

### Step 1 — tracking (`wcpy gen detlinegen`)

Generates ideal minimum-ionizing-particle depositions at a fixed angle.
Outputs `depos.npz` (energy depositions) and `depos.json` (track metadata
including angle and detector parameters used by `ssss-metrics`).

Detector-specific arguments are set by `tracking_params(w)`:
- `pdhd`, `pdsp`: `--offset '-1*m,0*m,0*m'` (horizontal drift, 1 m from anode in X)
- `pdvd`: `--offset '0*m,-1*m,0*m'` (vertical drift, 1 m from anode in Y)

The `--angle-coords=wire-plane` flag means `theta_xz` and `theta_y` are
interpreted in the coordinate system of the target wire plane.

### Step 2 — sim (`phlex` via `cfg/dune/phlex/job/sim.jsonnet`)

Drifts depositions through LAr, applies electronics response, adds noise,
and digitizes to ADC counts.  Wraps `cfg/dune/wct/job/sim.jsonnet`:

```
DepoSetBoundarySource → DepoSetDrifter → DepoTransform
  → Reframer → AddNoise → Digitizer → FrameBoundarySink
```

### Step 3 — splat (`phlex` via `cfg/dune/phlex/job/splat.jsonnet`)

Produces "true signal" frames via `DepoFluxSplat` for use as the truth
reference in metrics.  Reads the same `depos.npz` as sim (drift is
deterministic so the result is equivalent to shared drift with fan-out).
Wraps `cfg/dune/wct/job/splat.jsonnet`:

```
DepoSetBoundarySource → DepoSetDrifter → DepoFluxSplat
  → Reframer → FrameBoundarySink
```

`DepoFluxSplat` needs `smear_long` and `smear_tran` per wire plane; these
live in `det.splat` inside each `cfg/dune/wct/dets/{detector}/detector.jsonnet`.

### Step 4 — sigproc (`phlex` via `cfg/dune/phlex/job/sigproc.jsonnet`)

Runs OmnibusSigProc on the digitized frames.  Wraps
`cfg/dune/wct/job/sigproc.jsonnet`:

```
FrameFileSource → OmnibusSigProc → FrameFileSink
```

### Step 5 — metrics (`wcpy test ssss-metrics`)

Compares splat frames (truth) to signal frames.  Outputs a JSON file with
per-plane `ineff`, `fit.avg` (bias), `fit.sigma` (resolution), and the raw
charge-ratio histogram.  The `--params depos.json` argument supplies the
angle and detector metadata stored with the track.

### Step 6 — mplots (`wcpy test plot-ssss`)

Detailed per-angle comparison plots showing the splat and signal charge
distributions side by side.  One PDF per `(plane, theta_xz, theta_y)`.

### Step 7 — plots (`wcpy test plot-metrics`)

Summary plots across all track angles for all three wire planes.  Two PDFs
are produced per run:

- `*-wirecoords.pdf`: each plane uses its own wire-angle coordinate system
- `*-globalcoords.pdf`: all planes use the W-plane (index 2) global coordinate system

### PHLEX Jsonnet evaluation

`phlex` resolves Jsonnet imports only in the local directory and system
Jsonnet library paths — it does **not** search `WIRECELL_PATH`.  The workflow
therefore uses `wcsonnet` (the WCT-aware Jsonnet evaluator) to pre-evaluate
each PHLEX workflow Jsonnet to JSON, then passes the JSON to `phlex`:

```bash
wcsonnet --tla-code 'params={detname: "pdhd"}' \
         --tla-str depo_file="depos.npz" \
         --tla-str output_file="digits.npz" \
         --tla-code anode_index=0 \
         dune/phlex/job/sim.jsonnet \
         > digits.npz.workflow.json
phlex -c digits.npz.workflow.json
```

The `.workflow.json` files are left in the output directory.

### Environment auto-detection

When `WIRECELL_PATH` is not set explicitly, the script constructs it from:

1. `wire-cell-phlex/cfg/` — always included (Jsonnet library for `dune/` configs)
2. A sibling `wire-cell-data/` directory (peer of `wire-cell-phlex/`) — included
   if found; contains detector-specific data files that may not be in the installed
   prefix (e.g. `np04hd-garfield-6paths-mcmc-bestfit.json.bz2` for PDHD APA 0)
3. An install-prefix `data/` or `share/wirecell/` directory containing
   `wirecell.jsonnet` — searched under `/home/bviren/newsp/toolkit`, `/usr/local`, `/usr`
4. A `local/share/wirecell/` directory sibling to the repo — included if it
   contains `wirecell.jsonnet`

Both 3 and 4 are included when both are found, so `wirecell.jsonnet` and
detector data files are available regardless of which directory holds each.

`PHLEX_PLUGIN_PATH` is built from `wire-cell-phlex/build/` (for `wcp_*.so`
plugins) and the `lib/` directory alongside the `phlex` binary.

---

## Adding a new detector

To extend `spdir-phlex` to support a new detector `newdet`:

### 1. Create the WCT detector description

Add `cfg/dune/wct/dets/newdet/detector.jsonnet` following the pattern of
`pdhd/detector.jsonnet`.  The `splat` key is required:

```jsonnet
splat: {
    sparse:          true,
    tick:            daq_defaults.tick,
    window_start:    0,
    window_duration: daq_defaults.tick * daq_defaults.nticks,
    reference_time:  0.0,
    // Empirical extra-smear values for DepoFluxSplat, one per wire plane [U,V,W].
    // Derive with: wcpy gen morse-* analysis (see wire-cell-toolkit docs).
    // Use zeros as a conservative starting point — splat truth will be tighter
    // than what SP achieves, which biases efficiency slightly pessimistic.
    smear_long: [0.0, 0.0, 0.0],
    smear_tran: [0.0, 0.0, 0.0],
},
```

Register the detector in `cfg/dune/wct/dets.jsonnet`:

```jsonnet
local dets = {
    pdhd: import "dune/wct/dets/pdhd/detector.jsonnet",
    pdvd: import "dune/wct/dets/pdvd/detector.jsonnet",
    newdet: import "dune/wct/dets/newdet/detector.jsonnet",
};
```

### 2. Add tracking support in spdir-phlex

In `test/scripts/spdir-phlex`, update `tracking_params()` with the correct
`--offset` for the new detector's drift geometry:

```python
def tracking_params(w):
    det = w.detector
    if det in ("pdhd", "pdsp", "newdet"):
        return "--offset '-1*m,0*m,0*m'"   # horizontal drift
    if det == "pdvd":
        return "--offset '0*m,-1*m,0*m'"   # vertical drift
    return ""
```

The offset places the track center 1 m back from the wire plane in the drift
direction.  For horizontal-drift detectors this is `-X`; for vertical-drift
it is `-Y` (or `+Y` depending on orientation — verify empirically by checking
that depositions land within the drift volume).

If the detector name is not in `wire-cell-data/detectors.jsonnet`, add a
mapping in `detlinegen_detector()`:

```python
def detlinegen_detector(w):
    mapping = {"pdvd": "dune-vd", "newdet": "wct-registry-name"}
    return mapping.get(w.detector, w.detector)
```

### 3. Data files

The detector's wires, field response, and noise spectra files must be
reachable via `WIRECELL_PATH`.  If they live in a custom directory not found
by auto-detection, either:

- Place them in a sibling `wire-cell-data/` directory (auto-detected), or
- Pass `--config wirecell_path="cfg:/path/to/data"` explicitly.

### 4. DepoFluxSplat smear values

The `smear_long` and `smear_tran` values in `det.splat` represent the extra
charge spread introduced by the signal-processing chain and are subtracted
from the raw DepoFluxSplat output to produce a fair truth reference.

- For detectors sharing the Garfield field-response family with PDSP, the
  PDSP values (used for PDHD) are a reasonable starting point.
- For geometrically distinct detectors (e.g. PDVD), start with zeros and
  derive proper values by running the `wcpy gen morse-*` analysis on real or
  simulated data after SP is tuned.  Zero smearing gives a truth reference
  that is slightly tighter than the SP output, making efficiency look
  marginally pessimistic.

### 5. WCT job configs

The PHLEX job wrappers (`cfg/dune/phlex/job/{sim,splat,sigproc}.jsonnet`) and
WCT sub-graphs (`cfg/dune/wct/job/{sim,splat,sigproc}.jsonnet`) are
detector-agnostic — they take `detector` as a TLA and look up everything
through `dets[detector]`.  No changes are needed there once the detector
description is registered in `dets.jsonnet`.

---

## Troubleshooting

### `No factory for class FieldResponse`

`FieldResponse` is provided by `WireCellSigProc`.  Ensure `WireCellSigProc`
is in the `wct_plugins` list of every PHLEX module that uses a field response
file — including the `splat` module, which only uses the FR for drift
parameters (period and origin), not for full simulation.

### `lookup_tn<WireCell::IRandom>: Empty type:name string`

`DepoTransform` requires an explicit `rng` field in its configuration data.
This is already present in `cfg/dune/wct/job/sim.jsonnet`; if you copy that
file as a starting point for a new job config, do not drop the `rng` field.

### Field response file not found

WIRECELL_PATH must cover both the file containing `wirecell.jsonnet` and the
directory containing the detector-specific `.json.bz2` data files.  These are
sometimes in different directories.  The auto-detection logic searches for
both; if it misses your setup, set `WIRECELL_PATH` explicitly or pass
`--config wirecell_path=...`.

PDHD APA 0 uses `np04hd-garfield-6paths-mcmc-bestfit.json.bz2` which is not
in standard WCT install prefixes but is present in `wire-cell-data/` (the
companion data repository).  APAs 1–3 use `dune-garfield-1d565.json.bz2`
which is more widely distributed.

### Snakemake reports unexpected job counts

With Snakemake 9+, each `--config` flag replaces the entire config dict.
If you pass angle overrides as a separate `--config` they will silently drop
the `detector` override (or vice versa).  Always combine all overrides:

```bash
# Wrong (detector override is lost):
spdir-phlex --config detector=pdhd --config "theta_xz_deg=[45]"

# Correct:
spdir-phlex --config detector=pdhd "theta_xz_deg=[45]" "theta_y_deg=[90]"
```

### `KeyError: 'bias'` in plot-metrics

`plot-metrics` expects metrics from all three planes in each call.  This
error occurs when only a subset of planes produced metrics (e.g., running
with `planes=[0]` instead of the default `[0,1,2]`).  The `plots` rule in
`spdir-phlex` automatically gathers metrics for all planes, so this error
should not appear in a normal workflow run.
