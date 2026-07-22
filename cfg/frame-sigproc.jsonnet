// cfg/frame-sigproc.jsonnet
//
// WCT sub-graph: GenericFrameBoundarySource → OmnibusSigProc → GenericFrameBoundarySink.
//
// Standalone signal processing variant (Variant A of third-real-job).
// Used by the FrameFilter executor.  Each PHLEX event fills the boundary
// source with one input Frame, runs the graph, and drains the output Frame.
//
// Uses PDSP APA 0 geometry and PDSP signal processing parameters.
//
// Data files required (via WIRECELL_PATH):
//   protodune-wires-larsoft-v4.json.bz2  — wire geometry
//   dune-garfield-1d565.json.bz2         — field response
//
// TLA parameters:
//   source_name_0 (string): instance name for GenericFrameBoundarySource
//   sink_name_0   (string): instance name for GenericFrameBoundarySink
//   app_name    (string): instance name for Pgrapher
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellSigProc, WireCellAux

local wc = import "wirecell.jsonnet";
local spfilt = import "sp-filters.jsonnet";

function(
    source_name_0    = "wcphlex_frame_source",
    sink_name_0      = "wcphlex_frame_sink",
    app_name       = "wcphlex_pgrapher",
    service_prefix = "",   // prefix for all service component names;
                           // "" (default) = bare names, shared with any other island
                           // that uses the same default; non-empty = independent copies
)

local tick = 0.5 * wc.us;

// ---------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------

local dft = {
    type: "FftwDFT",
    name: service_prefix + "dft",
    data: {},
};

// ---------------------------------------------------------------------------
// Geometry and field response data files
// ---------------------------------------------------------------------------

local wires = {
    type: "WireSchemaFile",
    name: service_prefix + "wires",
    data: { filename: "protodune-wires-larsoft-v4.json.bz2" },
};

local fr = {
    type: "FieldResponse",
    name: service_prefix + "fr",
    data: { filename: "dune-garfield-1d565.json.bz2" },
};

// ---------------------------------------------------------------------------
// Electronics response (SP uses ColdElecResponse as IWaveform, not IWaveform[])
// ---------------------------------------------------------------------------

local elec = {
    type: "ColdElecResponse",
    name: service_prefix + "elec",
    data: {
        tick:     tick,
        nticks:   10125,
        shaping:  2.2 * wc.us,         // PDSP shaping time
        gain:     14.0 * wc.mV / wc.fC,
        postgain: 1.0,
    },
};

// ---------------------------------------------------------------------------
// PDSP APA 0 face geometry (same as deposet-drift-sim.jsonnet)
// ---------------------------------------------------------------------------

local faces = [
    { anode: -3578.36 * wc.mm, response: -3487.8875 * wc.mm, cathode: -1.5875 * wc.mm },
    { anode: -3683.14 * wc.mm, response: -3773.6125 * wc.mm, cathode: -7259.9125 * wc.mm },
];

local anode = {
    type: "AnodePlane",
    name: service_prefix + "apa0",
    data: {
        ident:       0,
        nimpacts:    10,
        wire_schema: wc.tn(wires),
        faces:       faces,
    },
};

// ---------------------------------------------------------------------------
// OmnibusSigProc
//
// ADC_mV: PDSP is 12-bit with 1.4V full-scale → 4095 / 1.4 V
// ctoffset: 1 us coarse time offset (PDSP standard)
// per_chan_resp: "" disables per-channel response (PDSP has no chresp file)
// ---------------------------------------------------------------------------

local sigproc = {
    type: "OmnibusSigProc",
    name: service_prefix + "sigproc",
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
        // ROI thresholds (PDSP defaults)
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
    type: "GenericFrameBoundarySource",
    name: source_name_0,
    data: {},
};

local snk = {
    type: "GenericFrameBoundarySink",
    name: sink_name_0,
    data: {},
};

// ---------------------------------------------------------------------------
// Full component list + Pgrapher
// ---------------------------------------------------------------------------

[dft, wires, fr, elec, anode] + spfilt + [sigproc, src, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            {
                tail: { node: wc.tn(src),      port: 0 },
                head: { node: wc.tn(sigproc),  port: 0 },
            },
            {
                tail: { node: wc.tn(sigproc),  port: 0 },
                head: { node: wc.tn(snk),       port: 0 },
            },
        ],
    },
}]
