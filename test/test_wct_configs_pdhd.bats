#!/usr/bin/env bats
# test/test_wct_configs_pdhd.bats
#
# Validate that wire-cell-phlex cfg/dune/ PDHD configs faithfully
# capture WCT component configuration from the dunereco / toolkit references.
#
# Each @test evaluates our cfg/dune/wct/job/*.jsonnet (APA 0) and a
# corresponding reference config, then uses compare_component.py to compare
# a single WCT component type.  WCT TN cross-references (e.g. data.rng,
# data.anode) are skipped because instance names legitimately differ;
# physics-bearing numeric fields must match.
#
# Prerequisites (set by direnv from plan/.envrc):
#   WIRECELL_PATH   — must include wire-cell-toolkit/cfg and wire-cell-phlex/cfg
#   DUNERECO_REPO   — path to dunereco git clone
#   wcsonnet        — in PATH (from local/bin via load_prefix $PWD/local)
#
# Run from wire-cell-phlex root:
#   bats test/test_wct_configs_pdhd.bats
#
# Or via:
#   cd wire-cell-phlex && bats test/test_wct_configs_pdhd.bats

# ---------------------------------------------------------------------------
# setup_file: evaluate configs once; shared across all tests in this file
# ---------------------------------------------------------------------------

setup_file() {
    TEST_DIR="$(cd "$(dirname "$BATS_TEST_FILENAME")" && pwd)"
    REPO_ROOT="$(cd "$TEST_DIR/.." && pwd)"
    export COMPARE_COMP="$TEST_DIR/compare_component.py"
    export DUNERECO_PDHD="$DUNERECO_REPO/dunereco/DUNEWireCell/pdhd"

    # Verify required tools and paths
    if ! command -v wcsonnet >/dev/null 2>&1; then
        skip "wcsonnet not found in PATH (source plan/.envrc via direnv)"
    fi
    if [[ -z "$WIRECELL_PATH" ]]; then
        skip "WIRECELL_PATH not set (source plan/.envrc via direnv)"
    fi
    if [[ -z "$DUNERECO_REPO" ]]; then
        skip "DUNERECO_REPO not set (source plan/.envrc via direnv)"
    fi
    if [[ ! -d "$DUNERECO_PDHD" ]]; then
        skip "DUNERECO_PDHD not found: $DUNERECO_PDHD"
    fi

    # Temporary directory for cached JSON results (shared across tests)
    export CONFIGS_TMPDIR
    CONFIGS_TMPDIR="$(mktemp -d)"

    # -------------------------------------------------------------------------
    # Evaluate our configs (APA 0)
    # -------------------------------------------------------------------------
    # Jsonnet evaluation runs from REPO_ROOT so relative imports work.
    (
        cd "$REPO_ROOT"

        wcsonnet \
            --tla-str detector=pdhd \
            --tla-str anode_index=0 \
            dune/wct/job/sim.jsonnet \
            > "$CONFIGS_TMPDIR/our_sim.json"

        wcsonnet \
            --tla-str detector=pdhd \
            --tla-str anode_index=0 \
            dune/wct/job/splat.jsonnet \
            > "$CONFIGS_TMPDIR/our_splat.json"

        wcsonnet \
            --tla-str detector=pdhd \
            --tla-str anode_index=0 \
            dune/wct/job/sigproc.jsonnet \
            > "$CONFIGS_TMPDIR/our_sigproc.json"
    )

    # -------------------------------------------------------------------------
    # Evaluate dunereco sim reference
    # Ext vars match our detector.jsonnet defaults (toolkit common/params defaults):
    #   DL=7.2 cm²/s, DT=12.0 cm²/s, lifetime=8.0 ms, driftSpeed=1.6 mm/μs.
    # NOTE: Production FHiCL (wirecell_dune.fcl) uses DL=4.0, DT=8.8,
    # lifetime=35.0, driftSpeed=1.565 (from LArSoft services).  Our defaults
    # are the WCT toolkit nominal values, not the tuned PDHD production values.
    # The spdir-phlex workflow tests SP performance at fixed nominal conditions
    # so the exact LAr properties do not affect SP metrics comparisons.
    # -------------------------------------------------------------------------
    wcsonnet \
        -P "$DUNERECO_PDHD" \
        --ext-code "DL=7.2" \
        --ext-code "DT=12.0" \
        --ext-code "lifetime=8.0" \
        --ext-code "driftSpeed=1.6" \
        "$DUNERECO_PDHD/wcls-sim-drift-simchannel-priorSCE.jsonnet" \
        > "$CONFIGS_TMPDIR/ref_sim.json"

    # -------------------------------------------------------------------------
    # Evaluate dunereco sigproc reference
    # Note: imports 'pgrapher/experiment/pdhd/sp.jsonnet' via WIRECELL_PATH,
    # which resolves to the toolkit's sp.jsonnet (not dunereco's sp.jsonnet).
    # -------------------------------------------------------------------------
    wcsonnet \
        -P "$DUNERECO_PDHD" \
        --ext-str "reality=sim" \
        --ext-str "epoch=perfect" \
        --ext-str "raw_input_label=daq" \
        --ext-str "signal_output_form=sparse" \
        --ext-code "clock_speed=2.0" \
        "$DUNERECO_PDHD/wcls-rawdigit-sp.jsonnet" \
        > "$CONFIGS_TMPDIR/ref_sigproc.json"
}

teardown_file() {
    if [[ -n "$CONFIGS_TMPDIR" && -d "$CONFIGS_TMPDIR" ]]; then
        rm -rf "$CONFIGS_TMPDIR"
    fi
}

# ---------------------------------------------------------------------------
# Helper: run compare_component.py and assert pass
# Usage: cmp_component TYPE OUR_JSON REF_JSON [--skip-field F ...]
# ---------------------------------------------------------------------------
cmp_component() {
    local ctype="$1"; shift
    local our_json="$1"; shift
    local ref_json="$1"; shift
    python3 "$COMPARE_COMP" --type "$ctype" "$our_json" "$ref_json" "$@"
}

# ===========================================================================
# Config evaluation smoke tests
# ===========================================================================

@test "wcsonnet evaluates our pdhd sim.jsonnet without error" {
    [[ -s "$CONFIGS_TMPDIR/our_sim.json" ]]
}

@test "wcsonnet evaluates our pdhd splat.jsonnet without error" {
    [[ -s "$CONFIGS_TMPDIR/our_splat.json" ]]
}

@test "wcsonnet evaluates our pdhd sigproc.jsonnet without error" {
    [[ -s "$CONFIGS_TMPDIR/our_sigproc.json" ]]
}

@test "wcsonnet evaluates dunereco sim reference without error" {
    [[ -s "$CONFIGS_TMPDIR/ref_sim.json" ]]
}

@test "wcsonnet evaluates dunereco sigproc reference without error" {
    [[ -s "$CONFIGS_TMPDIR/ref_sigproc.json" ]]
}

# ===========================================================================
# Sim components vs dunereco wcls-sim-drift-simchannel-priorSCE.jsonnet
# ===========================================================================

@test "Drifter: physics params match dunereco sim reference" {
    # Skip WCT TN cross-refs (data.rng).
    # Skip data.xregions: our config creates one Drifter per APA with 2 face
    # entries; the reference creates one Drifter for all APAs with all 4×2=4
    # face entries.  Geometry correctness is validated by the AnodePlane test.
    # Skip toolkit-specific optional fields (ar39activity, density, time_offset)
    # that the reference config sets but ours omits.
    cmp_component Drifter \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --skip-field data.rng \
        --skip-field data.xregions \
        --skip-field data.ar39activity \
        --skip-field data.density \
        --skip-field data.time_offset
}

@test "AnodePlane: matches dunereco sim reference" {
    # Skip wire_schema TN; ident/nimpacts/faces must match.
    cmp_component AnodePlane \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --skip-field data.wire_schema
}

@test "ColdElecResponse: matches dunereco sim reference" {
    # Skip optional filename field (ref may have filename:"", ours omits it).
    cmp_component ColdElecResponse \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --skip-field data.filename
}

@test "PlaneImpactResponse U-plane: matches dunereco sim reference" {
    # Skip TN fields for dft, field_response, short_responses (list of TNs).
    cmp_component PlaneImpactResponse \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --index-a 0 --index-b 0 \
        --skip-field data.dft \
        --skip-field data.field_response \
        --skip-field data.short_responses
}

@test "PlaneImpactResponse V-plane: matches dunereco sim reference" {
    cmp_component PlaneImpactResponse \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --index-a 1 --index-b 1 \
        --skip-field data.dft \
        --skip-field data.field_response \
        --skip-field data.short_responses
}

@test "PlaneImpactResponse W-plane: matches dunereco sim reference" {
    cmp_component PlaneImpactResponse \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --index-a 2 --index-b 2 \
        --skip-field data.dft \
        --skip-field data.field_response \
        --skip-field data.short_responses
}

@test "DepoTransform: physics params match dunereco sim reference" {
    # Skip TN cross-refs (rng, anode, pirs, dft).
    # Skip first_frame_number: our convention (0) vs LArSoft convention (100).
    # Physics fields: fluctuate, drift_speed, readout_time, start_time, tick, nsigma.
    cmp_component DepoTransform \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --skip-field data.rng \
        --skip-field data.anode \
        --skip-field data.pirs \
        --skip-field data.dft \
        --skip-field data.first_frame_number
}

@test "Reframer: matches dunereco sim reference" {
    # Skip anode TN; tbin/toffset/nticks/fill/tags must match.
    cmp_component Reframer \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --skip-field data.anode
}

@test "EmpiricalNoiseModel: matches dunereco sim reference" {
    # Skip anode and dft TNs.
    cmp_component EmpiricalNoiseModel \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --skip-field data.anode \
        --skip-field data.dft
}

@test "AddNoise: matches dunereco sim reference" {
    # Skip rng, dft, model TNs.
    cmp_component AddNoise \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --skip-field data.rng \
        --skip-field data.dft \
        --skip-field data.model
}

@test "Digitizer: matches dunereco sim reference" {
    # Skip anode TN and frame_tag (LArSoft-specific output tagging).
    cmp_component Digitizer \
        "$CONFIGS_TMPDIR/our_sim.json" \
        "$CONFIGS_TMPDIR/ref_sim.json" \
        --skip-field data.anode \
        --skip-field data.frame_tag
}

# ===========================================================================
# DepoFluxSplat — check key fields directly from our splat config
# (toolkit reference values, after splat window bug fix in detector.jsonnet)
# ===========================================================================

@test "DepoFluxSplat: window_start equals -312500 ns (tick0_time - response_plane/drift_speed)" {
    # Expected: -250000 - 62500 = -312500 ns
    local val
    val=$(python3 - "$CONFIGS_TMPDIR/our_splat.json" <<'EOF'
import json, sys
data = json.load(open(sys.argv[1]))
c = next(c for c in data if c["type"] == "DepoFluxSplat")
print(c["data"]["window_start"])
EOF
)
    # Allow small floating-point variation
    python3 -c "import sys; v=float('$val'); assert abs(v - -312500) < 1, f'window_start={v} != -312500'"
}

@test "DepoFluxSplat: window_duration equals 3062500 ns ((nticks+response_nticks)*tick)" {
    # Expected: (6000 + 125) * 500 = 3062500 ns
    local val
    val=$(python3 - "$CONFIGS_TMPDIR/our_splat.json" <<'EOF'
import json, sys
data = json.load(open(sys.argv[1]))
c = next(c for c in data if c["type"] == "DepoFluxSplat")
print(c["data"]["window_duration"])
EOF
)
    python3 -c "import sys; v=float('$val'); assert abs(v - 3062500) < 1, f'window_duration={v} != 3062500'"
}

@test "DepoFluxSplat: tick equals 500 ns (0.5 us)" {
    local val
    val=$(python3 - "$CONFIGS_TMPDIR/our_splat.json" <<'EOF'
import json, sys
data = json.load(open(sys.argv[1]))
c = next(c for c in data if c["type"] == "DepoFluxSplat")
print(c["data"]["tick"])
EOF
)
    python3 -c "import sys; v=float('$val'); assert abs(v - 500) < 1, f'tick={v} != 500'"
}

@test "DepoFluxSplat: smear_long matches toolkit PDHD values" {
    python3 - "$CONFIGS_TMPDIR/our_splat.json" <<'EOF'
import json, sys, math
data = json.load(open(sys.argv[1]))
c = next(c for c in data if c["type"] == "DepoFluxSplat")
expected = [2.691862363980221, 2.6750200122535057, 2.7137567141154055]
actual   = c["data"]["smear_long"]
assert len(actual) == 3, f"smear_long length {len(actual)} != 3"
for i, (a, e) in enumerate(zip(actual, expected)):
    assert math.isclose(a, e, rel_tol=1e-9), f"smear_long[{i}]={a} != {e}"
print("PASS")
EOF
}

@test "DepoFluxSplat: smear_tran matches toolkit PDHD values" {
    python3 - "$CONFIGS_TMPDIR/our_splat.json" <<'EOF'
import json, sys, math
data = json.load(open(sys.argv[1]))
c = next(c for c in data if c["type"] == "DepoFluxSplat")
expected = [0.7377218875719689, 0.7157764520393882, 0.13980698710556544]
actual   = c["data"]["smear_tran"]
assert len(actual) == 3, f"smear_tran length {len(actual)} != 3"
for i, (a, e) in enumerate(zip(actual, expected)):
    assert math.isclose(a, e, rel_tol=1e-9), f"smear_tran[{i}]={a} != {e}"
print("PASS")
EOF
}

@test "DepoFluxSplat: sparse=true and reference_time=0" {
    python3 - "$CONFIGS_TMPDIR/our_splat.json" <<'EOF'
import json, sys
data = json.load(open(sys.argv[1]))
c = next(c for c in data if c["type"] == "DepoFluxSplat")
assert c["data"]["sparse"] == True, "sparse is not True"
assert c["data"]["reference_time"] == 0.0, f"reference_time={c['data']['reference_time']} != 0"
print("PASS")
EOF
}

# ===========================================================================
# OmnibusSigProc vs dunereco wcls-rawdigit-sp.jsonnet
# (which uses toolkit sp.jsonnet values: r_th_factor=3.0, troi_col_th_factor=2.5)
# ===========================================================================

@test "OmnibusSigProc: physics params match dunereco/toolkit sigproc reference" {
    # Skip all WCT TN cross-refs: anode, dft, field_response, elecresponse,
    # filter_responses (list), per_chan_resp, Wiener filter names.
    # Skip output frame-tag fields that the reference sets for debugging /
    # HDF5 output but which our standalone config omits (e.g. gauss_tag,
    # loose_lf_tag, decon_charge_tag, etc.).
    # Skip mp_tick_resolution: reference may set this; ours omits it.
    # The reference resolves sp.jsonnet from WIRECELL_PATH → toolkit sp.jsonnet,
    # so r_th_factor=3.0 (all APAs) and troi_col_th_factor=2.5 in the reference.
    # Our detector.jsonnet now uses the same toolkit values.
    cmp_component OmnibusSigProc \
        "$CONFIGS_TMPDIR/our_sigproc.json" \
        "$CONFIGS_TMPDIR/ref_sigproc.json" \
        --skip-field data.anode \
        --skip-field data.dft \
        --skip-field data.field_response \
        --skip-field data.elecresponse \
        --skip-field data.filter_responses \
        --skip-field data.per_chan_resp \
        --skip-field data.Wiener_tight_filters \
        --skip-field data.wiener_filter_tight_U \
        --skip-field data.wiener_filter_tight_V \
        --skip-field data.wiener_filter_tight_W \
        --skip-field data.mp_tick_resolution \
        --skip-field data.gauss_tag \
        --skip-field data.loose_lf_tag \
        --skip-field data.tight_lf_tag \
        --skip-field data.decon_charge_tag \
        --skip-field data.wiener_tag \
        --skip-field data.cleanup_roi_tag \
        --skip-field data.break_roi_loop1_tag \
        --skip-field data.break_roi_loop2_tag \
        --skip-field data.shrink_roi_tag \
        --skip-field data.extend_roi_tag \
        --skip-field data.mp2_roi_tag \
        --skip-field data.mp3_roi_tag
}

# ===========================================================================
# Cross-check: sigproc from our sigproc.jsonnet contains OmnibusSigProc
# and has the expected r_th_factor and troi_col_th_factor values
# ===========================================================================

@test "OmnibusSigProc: r_th_factor=3.0 (toolkit value, all APAs)" {
    python3 - "$CONFIGS_TMPDIR/our_sigproc.json" <<'EOF'
import json, sys
data = json.load(open(sys.argv[1]))
c = next(c for c in data if c["type"] == "OmnibusSigProc")
v = c["data"]["r_th_factor"]
assert v == 3.0, f"r_th_factor={v}, expected 3.0 (toolkit sp.jsonnet value)"
print("PASS")
EOF
}

@test "OmnibusSigProc: troi_col_th_factor=2.5 (toolkit value)" {
    python3 - "$CONFIGS_TMPDIR/our_sigproc.json" <<'EOF'
import json, sys
data = json.load(open(sys.argv[1]))
c = next(c for c in data if c["type"] == "OmnibusSigProc")
v = c["data"]["troi_col_th_factor"]
assert v == 2.5, f"troi_col_th_factor={v}, expected 2.5 (toolkit sp.jsonnet value)"
print("PASS")
EOF
}
