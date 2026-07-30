// cfg/dune/wct/dets/pdvd/detector.jsonnet
//
// ProtoDUNE-VD (PDVD) detector description for wire-cell-phlex.
//
// Returns a plain-data object describing the PDVD detector.  No WCT component
// configs are embedded here (except sp_filters).  All WCT component construction
// happens in wct/job/*.jsonnet functions that consume this object.
//
// PDVD has vertical drift with 8 anodes: anodes 0-3 are "bottom drift" and
// anodes 4-7 are "top drift".  Bottom and top differ in electronics type,
// ADC settings, and noise spectra.
//
// Geometry source: protodunevd geometry (see wirecell-util wire-volumes).
// Transcribed from dunereco/DUNEWireCell/protodunevd/{params,simparams}.jsonnet.
//
// Face geometry: always sim-mode (both faces active per anode).
//
// Arguments:
//   params: override object.  Must contain params.detname = "pdvd".
//           Optional fields (deep-merged): lar, daq, sim.
//           Example: {detname:"pdvd", lar:{lifetime: 1000*wc.ms}}

local wc = import "wirecell.jsonnet";

function(params={detname: "pdvd"})

// ---------------------------------------------------------------------------
// Geometry constants
// ---------------------------------------------------------------------------
// Between center lines (APA-CPA distance for vertical drift)
local apa_cpa   = 341.55 * wc.cm;
local cpa_thick = 50.8   * wc.mm;
local apa_w2w   = 85.725 * wc.mm;
local plane_gap = 4.76   * wc.mm;
local apa_g2g   = 114.3  * wc.mm;  // slightly different from apa_w2w + 6*plane_gap due to rounding

// Anode cut-off: at the grid wires (outermost plane)
local apa_plane = 0.5 * apa_g2g;

// Response plane: distance from collection wires where Garfield calculations start.
// PDVD uses 18.1 cm (larger than PDHD's 10 cm due to different detector geometry).
// MUST match the field response files used.
local response_plane = 18.1 * wc.cm;

// Cathode cut-off
local cpa_plane = apa_cpa - 0.5 * cpa_thick;

// ---------------------------------------------------------------------------
// Per-anode entry builder
// ---------------------------------------------------------------------------
local make_anode(a) =
    // Anodes 0-3: bottom drift (sign=-1, drift in -Y direction)
    // Anodes 4-7: top drift    (sign=+1, drift in +Y direction)
    local sign       = if a > 3 then 1 else -1;
    local centerline = sign * apa_cpa;
    {
        ident: a,
        name: "anode%d" % a,

        // Both faces active (sim-mode geometry).
        // PDVD faces are symmetric: both faces of an anode have the same geometry.
        //
        // Coordinate note: the PDVD wire file maps physical Y (vertical drift axis)
        // to WCT X.  Anodes 0-3 sit at negative X (bottom CRPs), anodes 4-7 at
        // positive X (top CRPs), and the cathode is near X=0 (center).
        //
        // For bottom anodes (sign=-1, anode at X ≈ -347 cm):
        //   cathode = centerline - sign*cpa_plane ≈ -341.55 + 339.0 ≈ -2.5 cm  (center) ✓
        //   response = anode - sign*response_plane ≈ -347 + 18.1 ≈ -329 cm     (inside drift) ✓
        // For top anodes (sign=+1, anode at X ≈ +347 cm):
        //   cathode ≈ +2.5 cm  (center) ✓
        //   response ≈ +329 cm (inside drift) ✓
        local face = {
            anode:    centerline + sign * apa_plane,
            response: centerline + sign * (apa_plane - response_plane),
            cathode:  centerline - sign * cpa_plane,
        },
        faces: [ face, face ],

        // Electronics response.
        // Bottom drift (a < 4): standard ColdElecResponse
        // Top drift    (a >= 4): JsonElecResponse loaded from file
        elec: if a < 4 then {
            type:     "ColdElecResponse",
            gain:     7.8 * wc.mV / wc.fC,
            shaping:  2.2 * wc.us,
            postgain: 1.1365,
        } else {
            type:     "JsonElecResponse",
            filename: "dunevd-coldbox-elecresp-top-psnorm_400.json.bz2",
            postgain: 1.52,
        },

        // Field response file.
        // Both bottom and top drift use the same nominal PDVD field response.
        field: {
            filename: "protodunevd_FR_norminal_260324.json.bz2",
        },

        // No frequency-domain filter response for PDVD.
        filter_response: null,

        // Noise spectra.  Different files for bottom and top drift.
        noise: {
            filename: if a < 4
                then "pdvd-bottom-noise-spectra-v1.json.bz2"
                else "pdvd-top-noise-spectra-v2.json.bz2",
            wire_length_scale: 1.0 * wc.cm,
        },

        // ADC / digitizer settings.
        // Bottom and top drift use different baselines and fullscale ranges.
        adc: {
            resolution: 14,
            gain:       1.0,
            baselines: if a < 4
                then [1003.4 * wc.mV, 1003.4 * wc.mV, 507.7 * wc.mV]
                else [1.0 * wc.volt,  1.0 * wc.volt,  1.0 * wc.volt],
            fullscale: if a < 4
                then [0.2 * wc.volt, 1.6 * wc.volt]
                else [0.0 * wc.volt, 2.0 * wc.volt],
        },

        // OmnibusSigProc tuning.
        // PDVD uses ctoffset=4us and multi-plane protection; otherwise similar to PDHD.
        sigproc: {
            ctoffset:    4.0 * wc.us,
            ftoffset:    0.0,
            postgain:    1.0,
            fft_flag:    0,
            troi_col_th_factor: 5.0,
            troi_ind_th_factor: 3.0,
            lroi_rebin:         6,
            lroi_th_factor:     3.5,
            lroi_th_factor1:    0.7,
            lroi_jump_one_bin:  1,
            r_th_factor:  3.0,
            r_fake_signal_low_th:             375,
            r_fake_signal_high_th:            750,
            r_fake_signal_low_th_ind_factor:  1.0,
            r_fake_signal_high_th_ind_factor: 1.0,
            r_th_peak:    3.0,
            r_sep_peak:   6.0,
            r_low_peak_sep_threshold_pre: 1200,
            use_roi_debug_mode:         false,
            use_multi_plane_protection: true,
            isWrapped:    false,
            sparse:       true,
            // Standard plane ordering
            plane2layer:    [0, 1, 2],
            wiener_filters: ["Wiener_tight_U", "Wiener_tight_V", "Wiener_tight_W"],
        },
    };

// ---------------------------------------------------------------------------
// Apply overrides from params argument
// ---------------------------------------------------------------------------
local lar_defaults = {
    DL:          4.0   * wc.cm2 / wc.s,
    DT:          8.8   * wc.cm2 / wc.s,
    lifetime:    1000.0 * wc.ms,
    drift_speed: 1.473 * wc.mm / wc.us,
    efield:      500  * wc.volt / wc.cm,   // nominal drift field (segment sampler recombination)
};

local daq_defaults = {
    tick:   0.5  * wc.us,
    nticks: 6000,
};

local sim_defaults = {
    fluctuate:  true,
    fixed:      true,
    continuous: false,
    tick0_time: -250 * wc.us,
    nsigma:     3,
    nimpacts:   10,
};

// ---------------------------------------------------------------------------
// Return the detector description object
// ---------------------------------------------------------------------------
{
    name: "pdvd",

    daq: daq_defaults + std.get(params, "daq", {}),

    lar: lar_defaults + std.get(params, "lar", {}),

    sim: sim_defaults + std.get(params, "sim", {}),

    // Response plane distance from collection wires.
    // PDVD: 18.1 cm (vs PDHD: 10 cm).
    response_plane: response_plane,

    wires: {
        filename: "protodunevd-wires-larsoft-v3.json.bz2",
    },

    // 8 anodes: anode0..anode3 (bottom drift), anode4..anode7 (top drift)
    anodes: [make_anode(a) for a in std.range(0, 7)],

    // SP filter components (PDVD-tuned; names hard-coded in C++)
    sp_filters: import "sp-filters.jsonnet",

    // Optional response systematics (off by default)
    sys_status: false,
    sys_resp: {
        start:      0 * wc.us,
        magnitude:  1.0,
        time_smear: 1.0 * wc.us,
    },
    rc_resp: {
        width:     1.1 * wc.ms,
        rc_layers: 0,
    },

    // DepoFluxSplat parameters.
    // smear_long and smear_tran represent the extra longitudinal / transverse
    // spread that the full sim+SP chain induces beyond pure charge flux.
    // Values are per wire-plane: [U, V, W].
    // No empirically derived values exist for PDVD yet (requires "wcpy gen morse-*").
    // Using zeros for now (conservative: splat truth is tighter than SP output).
    splat: {
        sparse:          true,
        tick:            daq_defaults.tick,
        // window_start: extend backward from tick0_time by the field-response headroom
        // (same calculation as sim.jsonnet start_time).
        window_start:    sim_defaults.tick0_time - response_plane / lar_defaults.drift_speed,
        // window_duration: full DAQ window plus the field-response headroom ticks.
        window_duration: (daq_defaults.nticks + wc.roundToInt(response_plane / lar_defaults.drift_speed / daq_defaults.tick)) * daq_defaults.tick,
        reference_time:  0.0,
        smear_long: [0.0, 0.0, 0.0],
        smear_tran: [0.0, 0.0, 0.0],
    },

    // Overall detector bounding box (informational)
    bounds: {
        tail: wc.point(-3.15, -3.42, 0.0,  wc.m),
        head: wc.point( 3.13,  3.42, 3.04, wc.m),
    },
}
