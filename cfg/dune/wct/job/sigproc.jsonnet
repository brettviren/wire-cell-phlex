// cfg/dune/wct/job/sigproc.jsonnet
//
// WCT sub-graph: GenericFrameBoundarySource → OmnibusSigProc → GenericFrameBoundarySink
//
// Implements signal processing for a single anode of any DUNE detector
// described by a DetectorDescription object (see wct/dets/).
//
// The detector is selected by the `detector` TLA.  All OmnibusSigProc tuning
// parameters are read from det.anodes[anode_index].sigproc.  SP filter
// components are read from det.sp_filters (detector-tuned, C++ hard-coded names).
//
// TLA parameters:
//   source_name_0    (string): instance name for GenericFrameBoundarySource
//   sink_name_0      (string): instance name for GenericFrameBoundarySink
//   app_name       (string): instance name for Pgrapher
//   detector       (string): canonical detector name, e.g. "pdhd" or "pdvd"
//   anode_index    (string): anode index into det.anodes[], e.g. "0"
//   service_prefix (string): prefix for all WCT service component names
//
// Required WCT plugins: WireCellPgraph, WireCellSigProc, WireCellAux

local wc   = import "wirecell.jsonnet";
local dets = import "dune/wct/dets.jsonnet";

function(
    source_name_0    = "wcphlex_frame_source",
    sink_name_0      = "wcphlex_frame_sink",
    app_name       = "wcphlex_pgrapher",
    detector       = "pdhd",
    anode_index    = "0",
    service_prefix = "",
)

local det = dets[detector]({detname: detector});
local ai  = std.parseInt(anode_index);
local a   = det.anodes[ai];
local sp  = a.sigproc;

// ---------------------------------------------------------------------------
// Derived quantities
// ---------------------------------------------------------------------------

local tick      = det.daq.tick;
local nticks    = det.daq.nticks;

// ADC_mV: LSB in mV; used by OmnibusSigProc for signal normalization.
local adc_range = a.adc.fullscale[1] - a.adc.fullscale[0];
local adc_mv    = ((1 << a.adc.resolution) - 1.0) / adc_range;

// ---------------------------------------------------------------------------
// Service components
// ---------------------------------------------------------------------------

local dft = {
    type: "FftwDFT",
    name: service_prefix + "dft_" + a.name,
    data: {},
};

// ---------------------------------------------------------------------------
// Wire geometry
// ---------------------------------------------------------------------------

local wires = {
    type: "WireSchemaFile",
    name: service_prefix + "wires_" + a.name,
    data: { filename: det.wires.filename },
};

// ---------------------------------------------------------------------------
// Field response
// ---------------------------------------------------------------------------

local fr = {
    type: "FieldResponse",
    name: service_prefix + "fr_" + a.name,
    data: { filename: a.field.filename },
};

// ---------------------------------------------------------------------------
// Electronics response (polymorphic)
// ---------------------------------------------------------------------------

local response_nticks = wc.roundToInt(det.response_plane / det.lar.drift_speed / tick);
local nticks_ductor   = nticks + response_nticks;

local elec = {
    type: a.elec.type,
    name: service_prefix + "elec_" + a.name,
    data: {
        tick:   tick,
        nticks: nticks_ductor,
    } + (
        if a.elec.type == "ColdElecResponse" then {
            shaping:  a.elec.shaping,
            gain:     a.elec.gain,
            postgain: a.elec.postgain,
        } else if a.elec.type == "JsonElecResponse" then {
            filename: a.elec.filename,
            postgain: a.elec.postgain,
        } else error "Unknown elec type: " + a.elec.type
    ),
};

// ---------------------------------------------------------------------------
// AnodePlane
// ---------------------------------------------------------------------------

local anode = {
    type: "AnodePlane",
    name: service_prefix + a.name,
    data: {
        ident:       a.ident,
        nimpacts:    det.sim.nimpacts,
        wire_schema: wc.tn(wires),
        faces:       a.faces,
    },
};

// ---------------------------------------------------------------------------
// Optional filter responses (for SP deconvolution correction).
// Only present for anodes where a.filter_response != null (e.g. PDHD APA0).
// ---------------------------------------------------------------------------

local has_filter_response = a.filter_response != null;

local filter_response_comps = if has_filter_response then [
    {
        type: "FilterResponse",
        name: service_prefix + "fltresp_" + a.name + "_" + plane,
        data: {
            filename:  a.filter_response.filename,
            plane:     plane,
            wires:     wc.tn(wires),
        },
    }
    for plane in [0, 1, 2]
] else [];

local filter_response_tns = if has_filter_response
    then [wc.tn(filter_response_comps[p]) for p in [0, 1, 2]]
    else [];

// ---------------------------------------------------------------------------
// OmnibusSigProc
// ---------------------------------------------------------------------------

local sigproc = {
    type: "OmnibusSigProc",
    name: service_prefix + "sigproc_" + a.name,
    data: {
        anode:          wc.tn(anode),
        dft:            wc.tn(dft),
        field_response: wc.tn(fr),
        elecresponse:   wc.tn(elec),
        per_chan_resp:  "",
        filter_responses: filter_response_tns,
        ADC_mV:         adc_mv,
        ftoffset:       sp.ftoffset,
        ctoffset:       sp.ctoffset,
        postgain:       sp.postgain,
        fft_flag:       sp.fft_flag,
        troi_col_th_factor:               sp.troi_col_th_factor,
        troi_ind_th_factor:               sp.troi_ind_th_factor,
        lroi_rebin:                       sp.lroi_rebin,
        lroi_th_factor:                   sp.lroi_th_factor,
        lroi_th_factor1:                  sp.lroi_th_factor1,
        lroi_jump_one_bin:                sp.lroi_jump_one_bin,
        r_th_factor:                      sp.r_th_factor,
        r_fake_signal_low_th:             sp.r_fake_signal_low_th,
        r_fake_signal_high_th:            sp.r_fake_signal_high_th,
        r_fake_signal_low_th_ind_factor:  sp.r_fake_signal_low_th_ind_factor,
        r_fake_signal_high_th_ind_factor: sp.r_fake_signal_high_th_ind_factor,
        r_th_peak:                        sp.r_th_peak,
        r_sep_peak:                       sp.r_sep_peak,
        r_low_peak_sep_threshold_pre:     sp.r_low_peak_sep_threshold_pre,
        use_roi_debug_mode:               sp.use_roi_debug_mode,
        use_multi_plane_protection:       sp.use_multi_plane_protection,
        isWrapped:                        sp.isWrapped,
        sparse:                           sp.sparse,
        // Wiener filter names: per-anode selection from det.anodes[*].sigproc
        wiener_filter_tight_U: sp.wiener_filters[0],
        wiener_filter_tight_V: sp.wiener_filters[1],
        wiener_filter_tight_W: sp.wiener_filters[2],
        // Wire plane ordering for deconvolution kernel
        plane2layer: sp.plane2layer,
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

[dft, wires, fr, elec, anode] + filter_response_comps + det.sp_filters +
[sigproc, src, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            { tail: { node: wc.tn(src),     port: 0 },
              head: { node: wc.tn(sigproc), port: 0 } },
            { tail: { node: wc.tn(sigproc), port: 0 },
              head: { node: wc.tn(snk),      port: 0 } },
        ],
    },
}]
