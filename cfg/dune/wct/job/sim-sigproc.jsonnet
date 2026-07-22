// cfg/dune/wct/job/sim-sigproc.jsonnet
//
// WCT sub-graph: GenericDepoSetBoundarySource → DepoSetDrifter → DepoTransform
//                → Reframer → AddNoise → Digitizer → OmnibusSigProc
//                → GenericFrameBoundarySink
//
// Combined drift simulation + signal processing for a single anode.
// Service components (AnodePlane, DFT, FieldResponse, ElecResponse) are
// shared between the simulation and signal processing stages.
//
// TLA parameters:
//   source_name_0    (string): instance name for GenericDepoSetBoundarySource
//   sink_name_0      (string): instance name for GenericFrameBoundarySink
//   app_name       (string): instance name for Pgrapher
//   detector       (string): canonical detector name, e.g. "pdhd" or "pdvd"
//   anode_index    (string): anode index into det.anodes[], e.g. "0"
//   service_prefix (string): prefix for all WCT service component names
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellSigProc, WireCellAux

local wc   = import "wirecell.jsonnet";
local dets = import "dune/wct/dets.jsonnet";

function(
    source_name_0    = "wcphlex_deposet_source",
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
// Derived timing quantities
// ---------------------------------------------------------------------------
local tick             = det.daq.tick;
local nticks_daq       = det.daq.nticks;
local response_nticks  = wc.roundToInt(det.response_plane / det.lar.drift_speed / tick);
local nticks_ductor    = nticks_daq + response_nticks;
local readout_time     = nticks_ductor * tick;
local start_time       = det.sim.tick0_time - det.response_plane / det.lar.drift_speed;

// ---------------------------------------------------------------------------
// Service components (shared between sim and sigproc stages)
// ---------------------------------------------------------------------------

local dft = {
    type: "FftwDFT",
    name: service_prefix + "dft_" + a.name,
    data: {},
};

local rng = {
    type: "Random",
    name: service_prefix + "rng_" + a.name,
    data: { seed: 1 },
};

local wires_comp = {
    type: "WireSchemaFile",
    name: service_prefix + "wires_" + a.name,
    data: { filename: det.wires.filename },
};

local fr = {
    type: "FieldResponse",
    name: service_prefix + "fr_" + a.name,
    data: { filename: a.field.filename },
};

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

local anode = {
    type: "AnodePlane",
    name: service_prefix + a.name,
    data: {
        ident:       a.ident,
        nimpacts:    det.sim.nimpacts,
        wire_schema: wc.tn(wires_comp),
        faces:       a.faces,
    },
};

local pir(plane) = {
    type: "PlaneImpactResponse",
    name: service_prefix + "pir%d_" % plane + a.name,
    data: {
        plane:                 plane,
        dft:                   wc.tn(dft),
        field_response:        wc.tn(fr),
        nticks:                nticks_ductor,
        tick:                  tick,
        short_responses:       [wc.tn(elec)],
        overall_short_padding: 0.1 * wc.ms,
        long_responses:        [],
        long_padding:          1.5 * wc.ms,
    },
};
local pirs = [pir(p) for p in [0, 1, 2]];

// ---------------------------------------------------------------------------
// Sim-stage components
// ---------------------------------------------------------------------------

local drifter_comp = {
    type: "Drifter",
    name: service_prefix + "drifter_" + a.name,
    data: {
        rng:         wc.tn(rng),
        DL:          det.lar.DL,
        DT:          det.lar.DT,
        lifetime:    det.lar.lifetime,
        drift_speed: det.lar.drift_speed,
        fluctuate:   det.sim.fluctuate,
        xregions:    a.faces,
    },
};

local setdrifter = {
    type: "DepoSetDrifter",
    name: service_prefix + "deposet_drifter_" + a.name,
    data: { drifter: wc.tn(drifter_comp) },
};

local transform = {
    type: "DepoTransform",
    name: service_prefix + "transform_" + a.name,
    data: {
        rng:                wc.tn(rng),
        anode:              wc.tn(anode),
        pirs:               [wc.tn(p) for p in pirs],
        dft:                wc.tn(dft),
        fluctuate:          det.sim.fluctuate,
        drift_speed:        det.lar.drift_speed,
        readout_time:       readout_time,
        start_time:         start_time,
        tick:               tick,
        nsigma:             det.sim.nsigma,
        first_frame_number: 0,
    },
};

local reframer = {
    type: "Reframer",
    name: service_prefix + "reframer_" + a.name,
    data: {
        anode:   wc.tn(anode),
        tags:    [],
        fill:    0.0,
        tbin:    response_nticks,
        toffset: 0,
        nticks:  nticks_daq,
    },
};

local noise_model = {
    type: "EmpiricalNoiseModel",
    name: service_prefix + "noise_model_" + a.name,
    data: {
        anode:             wc.tn(anode),
        dft:               wc.tn(dft),
        chanstat:          "",
        spectra_file:      a.noise.filename,
        nsamples:          nticks_daq,
        period:            tick,
        wire_length_scale: a.noise.wire_length_scale,
    },
};

local addnoise = {
    type: "AddNoise",
    name: service_prefix + "addnoise_" + a.name,
    data: {
        rng:                    wc.tn(rng),
        dft:                    wc.tn(dft),
        model:                  wc.tn(noise_model),
        nsamples:               nticks_daq,
        replacement_percentage: 0.02,
    },
};

local digitizer = {
    type: "Digitizer",
    name: service_prefix + "digitizer_" + a.name,
    data: {
        anode:      wc.tn(anode),
        resolution: a.adc.resolution,
        gain:       a.adc.gain,
        fullscale:  a.adc.fullscale,
        baselines:  a.adc.baselines,
    },
};

// ---------------------------------------------------------------------------
// Sigproc-stage components
// ---------------------------------------------------------------------------

local adc_range = a.adc.fullscale[1] - a.adc.fullscale[0];
local adc_mv    = ((1 << a.adc.resolution) - 1.0) / adc_range;

local has_filter_response = a.filter_response != null;
local filter_response_comps = if has_filter_response then [
    {
        type: "FilterResponse",
        name: service_prefix + "fltresp_" + a.name + "_" + plane,
        data: {
            filename: a.filter_response.filename,
            plane:    plane,
            wires:    wc.tn(wires_comp),
        },
    }
    for plane in [0, 1, 2]
] else [];
local filter_response_tns = if has_filter_response
    then [wc.tn(filter_response_comps[p]) for p in [0, 1, 2]]
    else [];

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
        wiener_filter_tight_U: sp.wiener_filters[0],
        wiener_filter_tight_V: sp.wiener_filters[1],
        wiener_filter_tight_W: sp.wiener_filters[2],
        plane2layer: sp.plane2layer,
    },
};

// ---------------------------------------------------------------------------
// Boundary nodes
// ---------------------------------------------------------------------------

local src = {
    type: "GenericDepoSetBoundarySource",
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

[dft, rng, wires_comp, fr, elec, anode] + pirs + filter_response_comps + det.sp_filters +
[drifter_comp, setdrifter, transform, reframer, noise_model, addnoise, digitizer, sigproc,
 src, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            { tail: { node: wc.tn(src),        port: 0 },
              head: { node: wc.tn(setdrifter),  port: 0 } },
            { tail: { node: wc.tn(setdrifter),  port: 0 },
              head: { node: wc.tn(transform),   port: 0 } },
            { tail: { node: wc.tn(transform),   port: 0 },
              head: { node: wc.tn(reframer),    port: 0 } },
            { tail: { node: wc.tn(reframer),    port: 0 },
              head: { node: wc.tn(addnoise),    port: 0 } },
            { tail: { node: wc.tn(addnoise),    port: 0 },
              head: { node: wc.tn(digitizer),   port: 0 } },
            { tail: { node: wc.tn(digitizer),   port: 0 },
              head: { node: wc.tn(sigproc),     port: 0 } },
            { tail: { node: wc.tn(sigproc),     port: 0 },
              head: { node: wc.tn(snk),          port: 0 } },
        ],
    },
}]
