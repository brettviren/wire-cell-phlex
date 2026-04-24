// cfg/dune/wct/dets/pdhd/detector.jsonnet
//
// ProtoDUNE-HD (PDHD) detector description for wire-cell-phlex.
//
// Returns a plain-data object describing the PDHD detector.  No WCT component
// configs are embedded here (except sp_filters which are WCT components by
// necessity of C++ hard-coded names).  All WCT component construction happens
// in wct/job/*.jsonnet functions that consume this object.
//
// Geometry source: protodunehd_v6 GDML.
// Transcribed from dunereco/DUNEWireCell/pdhd/{params,simparams}.jsonnet.
//
// Face geometry: always sim-mode (both faces active per APA).  Data-mode jobs
// that only need one face per APA can filter the faces array.
//
// Arguments:
//   params: override object.  Must contain params.detname = "pdhd".
//           Optional fields (deep-merged): lar, daq, sim, elec_gain.
//           Example: {detname:"pdhd", lar:{lifetime: 35*wc.ms}}

local wc = import "wirecell.jsonnet";

function(params={detname: "pdhd"})

// ---------------------------------------------------------------------------
// Geometry constants (from protodunehd_v6 GDML)
// ---------------------------------------------------------------------------
local apa_cpa   = 3.5734  * wc.m;
local cpa_thick = 3.175   * wc.mm;  // 1/8", confirmed with LArSoft
local apa_w2w   = 85.87   * wc.mm;
local plane_gap = 4.76    * wc.mm;
local apa_g2g   = apa_w2w + 6 * plane_gap;

// Anode cut-off: at the first induction wires (innermost plane)
local apa_plane = 0.5 * apa_g2g - plane_gap;

// Response plane: distance from collection wires where Garfield calculations start.
// MUST match the field response files used.
local response_plane = 10 * wc.cm;
local res_plane      = 0.5 * apa_w2w + response_plane;

// Cathode cut-off
local cpa_plane = apa_cpa - 0.5 * cpa_thick;

// ---------------------------------------------------------------------------
// Default electronics gain.  Override via params.elec_gain.
// Two standard values in use at PDHD: 14 mV/fC and 7.8 mV/fC.
// ---------------------------------------------------------------------------
local elec_gain = std.get(params, "elec_gain", 14.0) * wc.mV / wc.fC;

// ---------------------------------------------------------------------------
// Assemble per-anode entries
// ---------------------------------------------------------------------------
local make_anode(n) =
    local sign      = 2 * (n % 2) - 1;  // APA0,2 → -1; APA1,3 → +1
    local centerline = sign * apa_cpa;
    {
        ident: n,
        name: "apa%d" % n,

        // Both faces active (sim-mode geometry).
        // Face 0 is the "positive" face (higher X); face 1 is the "negative" face.
        // For APA0/2 (sign=-1): face0 is toward cathode side, face1 is toward cryo wall.
        // For APA1/3 (sign=+1): face0 is toward cryo wall, face1 is toward cathode side.
        faces: [
            {
                anode:    centerline + apa_plane,
                response: centerline + res_plane,
                cathode:  centerline + cpa_plane,
            },
            {
                anode:    centerline - apa_plane,
                response: centerline - res_plane,
                cathode:  centerline - cpa_plane,
            },
        ],

        // Electronics response.
        // All PDHD APAs use ColdElecResponse with the same parameters.
        elec: {
            type:     "ColdElecResponse",
            gain:     elec_gain,
            shaping:  2.2 * wc.us,
            postgain: 1.0,
        },

        // Field response file.
        // APA0 uses a higher-quality 6-path MCMC best-fit response.
        // APAs 1-3 use the generic DUNE 1D response.
        field: {
            filename: if n == 0
                then "np04hd-garfield-6paths-mcmc-bestfit.json.bz2"
                else "dune-garfield-1d565.json.bz2",
        },

        // Optional frequency-domain filter response for SP deconvolution correction.
        // Only applied to APA0 (the one with the special field response).
        filter_response: if n == 0
            then { filename: "protodunehd-field-response-filters.json.bz2" }
            else null,

        // Noise spectra.  File depends on the FE amplifier gain setting.
        noise: {
            filename: if elec_gain > 8 * wc.mV / wc.fC
                then "protodunehd-noise-spectra-14mVfC-v1.json.bz2"
                else "protodunehd-noise-spectra-7d8mVfC-v1.json.bz2",
            wire_length_scale: 1.0 * wc.cm,
        },

        // ADC / digitizer settings.
        // Uniform across all PDHD APAs.
        adc: {
            resolution: 14,
            gain:       1.0,
            baselines:  [1003.4 * wc.mV, 1003.4 * wc.mV, 507.7 * wc.mV],
            fullscale:  [0.2 * wc.volt, 1.6 * wc.volt],
        },

        // OmnibusSigProc tuning.
        // APA0 differs from APAs 1-3 in: plane2layer and Wiener filter names.
        //
        // NOTE on r_th_factor / troi_col_th_factor:
        // dunereco/DUNEWireCell/pdhd/sp.jsonnet (in the dunereco repo) uses
        //   r_th_factor=2.5 for APA0 and troi_col_th_factor=5.0.
        // However, wcls-rawdigit-sp.jsonnet imports sp.jsonnet via the absolute
        // WIRECELL_PATH path 'pgrapher/experiment/pdhd/sp.jsonnet', which
        // resolves to the toolkit's sp.jsonnet (not dunereco's).
        // The toolkit sp.jsonnet has r_th_factor=3.0 for all APAs and
        // troi_col_th_factor=2.5.  We use the toolkit values here because that
        // is what production LArSoft jobs actually execute.
        sigproc: {
            ctoffset:    1.0 * wc.us,
            ftoffset:    0.0,
            postgain:    1.0,
            fft_flag:    0,
            troi_col_th_factor: 2.5,
            troi_ind_th_factor: 3.0,
            lroi_rebin:         6,
            lroi_th_factor:     3.5,
            lroi_th_factor1:    0.7,
            lroi_jump_one_bin:  1,
            r_th_factor: 3.0,
            r_fake_signal_low_th:             375,
            r_fake_signal_high_th:            750,
            r_fake_signal_low_th_ind_factor:  1.0,
            r_fake_signal_high_th_ind_factor: 1.0,
            r_th_peak:   3.0,
            r_sep_peak:  6.0,
            r_low_peak_sep_threshold_pre: 1200,
            use_roi_debug_mode:         false,
            use_multi_plane_protection: false,
            isWrapped:   false,
            sparse:      true,
            // Wire plane ordering fed into the SP deconvolution kernel.
            // APA0 uses [0,2,1] (U,W,V); others use standard [0,1,2] (U,V,W).
            plane2layer: if n == 0 then [0, 2, 1] else [0, 1, 2],
            // Wiener filter instance names (hard-coded in OmnibusSigProc C++).
            // APA0 uses the "APA1-variant" names which have different tuning values
            // appropriate for the higher-quality 6-path MCMC field response.
            wiener_filters: if n == 0
                then ["Wiener_tight_U_APA1", "Wiener_tight_V_APA1", "Wiener_tight_W_APA1"]
                else ["Wiener_tight_U",      "Wiener_tight_V",      "Wiener_tight_W"],
        },
    };

// ---------------------------------------------------------------------------
// Apply overrides from params argument
// ---------------------------------------------------------------------------
local lar_defaults = {
    DL:          7.2  * wc.cm2 / wc.s,
    DT:          12.0 * wc.cm2 / wc.s,
    lifetime:    8    * wc.ms,
    drift_speed: 1.6  * wc.mm / wc.us,
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
    name: "pdhd",

    daq: daq_defaults + std.get(params, "daq", {}),

    lar: lar_defaults + std.get(params, "lar", {}),

    sim: sim_defaults + std.get(params, "sim", {}),

    // Response plane distance from collection wires.
    // Tied to the field response files; MUST match Garfield calculation origin.
    response_plane: response_plane,

    wires: {
        filename: "protodunehd-wires-larsoft-v1.json.bz2",
    },

    // 4 APAs: apa0 .. apa3
    anodes: [make_anode(n) for n in std.range(0, 3)],

    // SP filter components (detector-tuned; names hard-coded in C++)
    sp_filters: import "sp-filters.jsonnet",

    // Optional response systematics (off by default)
    sys_status: false,
    sys_resp: {
        start:      -10 * wc.us,
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
    // Determined empirically via "wcpy gen morse-*"; PDHD reuses PDSP values
    // (same field-response family).
    splat: {
        sparse:          true,
        tick:            daq_defaults.tick,
        // window_start: extend backward from tick0_time by the field-response headroom
        // (same calculation as sim.jsonnet start_time).
        window_start:    sim_defaults.tick0_time - response_plane / lar_defaults.drift_speed,
        // window_duration: full DAQ window plus the field-response headroom ticks.
        window_duration: (daq_defaults.nticks + wc.roundToInt(response_plane / lar_defaults.drift_speed / daq_defaults.tick)) * daq_defaults.tick,
        reference_time:  0.0,
        smear_long: [2.691862363980221, 2.6750200122535057, 2.7137567141154055],
        smear_tran: [0.7377218875719689, 0.7157764520393882, 0.13980698710556544],
    },

    // Overall detector bounding box (informational; for kinematics generators etc.)
    bounds: {
        tail: wc.point(-4.0, 0.0, 0.0, wc.m),
        head: wc.point(+4.0, 6.1, 7.0, wc.m),
    },
}
