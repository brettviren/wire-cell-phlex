// cfg/dune/wct/dets/pdsp/detector.jsonnet
//
// ProtoDUNE-SP (PDSP) detector description (runtime / self-contained variant).
//
// Returns a plain-data detector description consumed by dune/wct/job/*.jsonnet.
// This is the self-contained form used by the wire-cell-phlex runtime cfg tree
// (no dune/wct/lib schema import); the canonical schema-validated twin lives at
// devel/dune-config/cfg/dune/wct/detectors/pdsp/detector.jsonnet.
//
// Geometry source: protodune-wires-larsoft-v4 (LArSoft).
// Transcribed from pgrapher/experiment/pdsp/{params,simparams,sp,sp-filters}.jsonnet.
// lar values match test/test/test-pdsp-simsn-nfsp.jsonnet so a phlex PDSP job is
// directly comparable to that WCT reference (xerosere ddm-3bu.9).
//
// PDSP is the 6-APA sibling of PDHD: ONE generic 1D field response for every APA,
// no SP filter_response correction, standard plane ordering, standard Wiener names.

local wc = import "wirecell.jsonnet";

function(params={detname: "pdsp"})

// --- geometry (protodune-wires-larsoft-v4; pdsp/simparams.jsonnet) -----------
local apa_cpa   = 3.63075 * wc.m;
local cpa_thick = 3.175   * wc.mm;
local apa_w2w   = 85.725  * wc.mm;
local plane_gap = 4.76    * wc.mm;
local apa_g2g   = 114.3   * wc.mm;
local apa_plane = 0.5 * apa_g2g - plane_gap;   // first induction wires (sim-mode)

local response_plane = 10 * wc.cm;
local res_plane      = 0.5 * apa_w2w + response_plane;
local cpa_plane      = apa_cpa - 0.5 * cpa_thick;

local elec_gain = std.get(params, "elec_gain", 14.0) * wc.mV / wc.fC;

local make_anode(n) =
    local sign       = 2 * (n % 2) - 1;   // APA0,2,4 -> -1; APA1,3,5 -> +1
    local centerline = sign * apa_cpa;
    {
        ident: n,
        name: "apa%d" % n,
        faces: [
            { anode: centerline + apa_plane, response: centerline + res_plane, cathode: centerline + cpa_plane },
            { anode: centerline - apa_plane, response: centerline - res_plane, cathode: centerline - cpa_plane },
        ],
        elec: {
            type:     "ColdElecResponse",
            gain:     elec_gain,
            shaping:  2.2 * wc.us,
            postgain: 1.1365,   // pulser calibration (pdsp/params)
        },
        field: { filename: "dune-garfield-1d565.json.bz2" },
        filter_response: null,
        noise: {
            filename: "protodune-noise-spectra-v1.json.bz2",
            wire_length_scale: 1.0 * wc.cm,
        },
        adc: {
            resolution: 12,
            gain:       1.0,
            baselines:  [1003.4 * wc.mV, 1003.4 * wc.mV, 507.7 * wc.mV],
            fullscale:  [0.2 * wc.volt, 1.6 * wc.volt],
        },
        // OmnibusSigProc tuning (pgrapher/experiment/pdsp/sp.jsonnet, "Optimized May 2019").
        sigproc: {
            ctoffset:    1.0 * wc.us,
            ftoffset:    0.0,
            postgain:    1.0,
            fft_flag:    0,
            troi_col_th_factor: 5.0,
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
            plane2layer:    [0, 1, 2],
            wiener_filters: ["Wiener_tight_U", "Wiener_tight_V", "Wiener_tight_W"],
        },
    };

local lar_defaults = {
    DL:          4.0  * wc.cm2 / wc.s,
    DT:          8.8  * wc.cm2 / wc.s,
    lifetime:    10.4 * wc.ms,
    drift_speed: 1.565 * wc.mm / wc.us,
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

local lar = lar_defaults + std.get(params, "lar", {});
local sim = sim_defaults + std.get(params, "sim", {});
local daq = daq_defaults + std.get(params, "daq", {});

// --- plain-data detector description -----------------------------------------
{
    name: "pdsp",
    sys_status: false,

    daq: daq,
    lar: lar,
    sim: sim,

    response_plane: response_plane,

    wires: { filename: "protodune-wires-larsoft-v4.json.bz2" },

    anodes: [make_anode(n) for n in std.range(0, 5)],

    sp_filters: import "sp-filters.jsonnet",

    sys_resp: { start: -10 * wc.us, magnitude: 1.0, time_smear: 1.0 * wc.us },
    rc_resp:  { width: 1.1 * wc.ms, rc_layers: 1 },

    splat: {
        sparse:          true,
        tick:            daq.tick,
        window_start:    sim.tick0_time - response_plane / lar.drift_speed,
        window_duration: (daq.nticks + wc.roundToInt(response_plane / lar.drift_speed / daq.tick)) * daq.tick,
        reference_time:  0.0,
        smear_long: [2.691862363980221, 2.6750200122535057, 2.7137567141154055],
        smear_tran: [0.7377218875719689, 0.7157764520393882, 0.13980698710556544],
    },

    bounds: {
        tail: wc.point(-4.0, 0.0, 0.0, wc.m),
        head: wc.point(+4.0, 6.1, 7.0, wc.m),
    },
}
