// cfg/pdhd-apa-sim-sigproc.jsonnet
//
// WCT sub-graph: DepoSetBoundarySource → DepoTransform → OmnibusSigProc → FrameBoundarySink
//
// Implements per-APA electronics simulation + signal processing for one PDHD APA.
// The Drifter has already been applied upstream (in pdhd-file-drifter.jsonnet).
// DepoTransform projects drifted depos onto this APA's wire planes; depos outside
// this APA's AnodePlane are silently ignored.
//
// Parameterized by the apa_ident TLA (string "0"–"3").  PHLEX 0.2.0 only supports
// string TLAs, so the APA number is looked up from a map rather than computed
// arithmetically.
//
// Wire schema: protodunehd-wires-larsoft-v1.json.bz2
// Field response: dune-garfield-1d565.json.bz2 (generic; PDHD-specific response
//   not available in the local Spack view)
//
// PDHD geometry constants from wire-cell-toolkit simparams.jsonnet:
//   apa_cpa   = 3.5734 m     (APA-to-CPA distance / APA centerline magnitude)
//   apa_plane = 52.455 mm    (from APA centerline to outermost wire plane)
//   res_plane = 142.935 mm   (from APA centerline to response plane)
//   cpa_plane = 3571.8125 mm (from APA centerline to cathode)
//   nticks    = 6000 at 0.5 us (3 ms readout)
//
// TLA parameters:
//   source_name (string): instance name for DepoSetBoundarySource
//   sink_name   (string): instance name for FrameBoundarySink
//   app_name    (string): instance name for Pgrapher
//   apa_ident   (string): APA number "0", "1", "2", or "3"
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellSigProc, WireCellAux

local wc = import "wirecell.jsonnet";
local spfilt = import "sp-filters.jsonnet";

function(
    source_name = "wcphlex_deposet_source",
    sink_name   = "wcphlex_frame_sink",
    app_name    = "wcphlex_pgrapher",
    apa_ident   = "0",
)

local tick          = 0.5 * wc.us;
local nticks_ductor = 6000;
local readout_time  = nticks_ductor * tick;
local start_time    = -62.5 * wc.us;
local drift_speed   = 1.6 * wc.mm / wc.us;

// ---------------------------------------------------------------------------
// PDHD geometry constants
// ---------------------------------------------------------------------------

local apa_cpa   = 3.5734  * wc.m;
local cpa_thick = 3.175   * wc.mm;
local apa_w2w   = 85.87   * wc.mm;
local plane_gap = 4.76    * wc.mm;
local apa_g2g   = apa_w2w + 6 * plane_gap;
local apa_plane = 0.5 * apa_g2g - plane_gap;
local res_dist  = 10 * wc.cm;
local res_plane = 0.5 * apa_w2w + res_dist;
local cpa_plane = apa_cpa - 0.5 * cpa_thick;

// Pre-computed faces for each APA (n%2==0 → sign=-1, n%2==1 → sign=+1)
local all_faces = {
    "0": local cl = -apa_cpa;
         [ { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
           { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane } ],
    "1": local cl = +apa_cpa;
         [ { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
           { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane } ],
    "2": local cl = -apa_cpa;
         [ { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
           { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane } ],
    "3": local cl = +apa_cpa;
         [ { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
           { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane } ],
};

local all_idents = { "0": 0, "1": 1, "2": 2, "3": 3 };

local faces    = all_faces[apa_ident];
local apa_num  = all_idents[apa_ident];

// ---------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------

local dft = {
    type: "FftwDFT",
    name: "dft_apa" + apa_ident,
    data: {},
};

// ---------------------------------------------------------------------------
// Geometry and field response data files
// ---------------------------------------------------------------------------

local wires = {
    type: "WireSchemaFile",
    name: "wires_apa" + apa_ident,
    data: { filename: "protodunehd-wires-larsoft-v1.json.bz2" },
};

local fr = {
    type: "FieldResponse",
    name: "fr_apa" + apa_ident,
    data: { filename: "dune-garfield-1d565.json.bz2" },
};

// ---------------------------------------------------------------------------
// Electronics response
// ---------------------------------------------------------------------------

local elec = {
    type: "ColdElecResponse",
    name: "elec_apa" + apa_ident,
    data: {
        tick:     tick,
        nticks:   nticks_ductor,
        shaping:  2.2 * wc.us,
        gain:     14.0 * wc.mV / wc.fC,
        postgain: 1.0,
    },
};

// ---------------------------------------------------------------------------
// AnodePlane for this APA
// ---------------------------------------------------------------------------

local anode = {
    type: "AnodePlane",
    name: "apa" + apa_ident,
    data: {
        ident:       apa_num,
        nimpacts:    10,
        wire_schema: wc.tn(wires),
        faces:       faces,
    },
};

// ---------------------------------------------------------------------------
// Plane impact responses (one per plane: U=0, V=1, W=2)
// ---------------------------------------------------------------------------

local pir(plane) = {
    type: "PlaneImpactResponse",
    name: "pir%d_apa" % [plane] + apa_ident,
    data: {
        plane:                  plane,
        dft:                    wc.tn(dft),
        field_response:         wc.tn(fr),
        nticks:                 nticks_ductor,
        tick:                   tick,
        short_responses:        [wc.tn(elec)],
        overall_short_padding:  0.1 * wc.ms,
        long_responses:         [],
        long_padding:           1.5 * wc.ms,
    },
};
local pirs = [pir(n) for n in [0, 1, 2]];

// ---------------------------------------------------------------------------
// DepoTransform
// ---------------------------------------------------------------------------

local transform = {
    type: "DepoTransform",
    name: "transform_apa" + apa_ident,
    data: {
        anode:              wc.tn(anode),
        pirs:               [wc.tn(p) for p in pirs],
        dft:                wc.tn(dft),
        fluctuate:          false,
        drift_speed:        drift_speed,
        readout_time:       readout_time,
        start_time:         start_time,
        tick:               tick,
        nsigma:             3,
        first_frame_number: 0,
    },
};

// ---------------------------------------------------------------------------
// OmnibusSigProc
// ---------------------------------------------------------------------------

local sigproc = {
    type: "OmnibusSigProc",
    name: "sigproc_apa" + apa_ident,
    data: {
        anode:          wc.tn(anode),
        dft:            wc.tn(dft),
        field_response: wc.tn(fr),
        elecresponse:   wc.tn(elec),
        per_chan_resp:   "",
        ftoffset:       0.0,
        ctoffset:       1.0 * wc.us,
        postgain:       1.0,
        ADC_mV:         4095.0 / (1.4 * wc.volt),
        fft_flag:       0,
        troi_col_th_factor: 5.0,
        troi_ind_th_factor: 3.0,
        lroi_rebin:         6,
        lroi_th_factor:     3.5,
        lroi_th_factor1:    0.7,
        lroi_jump_one_bin:  1,
        r_th_factor:        3.0,
        r_fake_signal_low_th:    375,
        r_fake_signal_high_th:   750,
        r_fake_signal_low_th_ind_factor:  1.0,
        r_fake_signal_high_th_ind_factor: 1.0,
        r_th_peak:      3.0,
        r_sep_peak:     6.0,
        r_low_peak_sep_threshold_pre: 1200,
        use_roi_debug_mode:     false,
        use_multi_plane_protection: false,
        isWrapped:      false,
        sparse:         true,
    },
};

// ---------------------------------------------------------------------------
// Boundary nodes
// ---------------------------------------------------------------------------

local src = {
    type: "DepoSetBoundarySource",
    name: source_name,
    data: {},
};

local snk = {
    type: "FrameBoundarySink",
    name: sink_name,
    data: {},
};

// ---------------------------------------------------------------------------
// Full component list + Pgrapher
// ---------------------------------------------------------------------------

[dft, wires, fr, elec, anode] + pirs + spfilt + [transform, sigproc, src, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            {
                tail: { node: wc.tn(src),       port: 0 },
                head: { node: wc.tn(transform),  port: 0 },
            },
            {
                tail: { node: wc.tn(transform),  port: 0 },
                head: { node: wc.tn(sigproc),    port: 0 },
            },
            {
                tail: { node: wc.tn(sigproc),    port: 0 },
                head: { node: wc.tn(snk),         port: 0 },
            },
        ],
    },
}]
