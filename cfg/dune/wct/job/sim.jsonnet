// cfg/dune/wct/job/sim.jsonnet
//
// WCT sub-graph: GenericDepoSetBoundarySource → DepoSetDrifter → DepoTransform
//                → Reframer → AddNoise → Digitizer → GenericFrameBoundarySink
//
// Implements a full drift + electronics simulation for a single anode of any
// DUNE detector described by a DetectorDescription object (see wct/dets/).
//
// The detector is selected by the `detector` TLA (canonical name string).  The
// per-anode configuration is taken from det.anodes[anode_index].
//
// TLA parameters:
//   sources  — array of WCT inode { type, name } for the boundary source(s)
//   sinks    — array of WCT inode { type, name } for the boundary sink(s)
//   app_name      (string): instance name for Pgrapher
//   detector      (string): canonical detector name, e.g. "pdhd" or "pdvd"
//   anode_index   (string): anode index into det.anodes[], e.g. "0"
//   service_prefix (string): prefix for all WCT service component names;
//                  "" = bare names (shared across islands if multiple executors
//                  load configs with the same names); non-empty = independent
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellSigProc, WireCellAux

local wc  = import "wirecell.jsonnet";
local dets = import "dune/wct/dets.jsonnet";

function(
    sources  = [],
    sinks    = [],
    app_name       = "wcphlex_pgrapher",
    detector       = "pdhd",
    anode_index    = "0",
    service_prefix = "",
)

local det = dets[detector]({detname: detector});
local ai  = std.parseInt(anode_index);
local a   = det.anodes[ai];

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
// Service components
// ---------------------------------------------------------------------------

local dft = {
    type: "FftwDFT",
    name: service_prefix + "dft_" + a.name,
    data: {},
};

local rng = {
    type: "Random",
    name: service_prefix + "rng_" + a.name,
    data: { seed: 1 },   // fixed seed for reproducible noise
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
// Electronics response (polymorphic: ColdElecResponse or JsonElecResponse)
// ---------------------------------------------------------------------------

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
// Plane impact responses (one per wire plane: U=0, V=1, W=2)
// ---------------------------------------------------------------------------

// overall_short_padding must exceed the full field-response duration so that
// PlaneImpactResponse captures the entire response without truncation.
// The FR duration ≈ (response_plane / drift_speed) × 1.5 is an empirical
// upper bound: PDHD FR = 100 μs, PDVD FR = 132.5 μs (with origin=181 mm
// and transit time 118 μs). Using 2× response_plane/drift_speed gives
// 125 μs (PDHD) and 246 μs (PDVD), safely covering both.
local pir_short_padding = det.response_plane / det.lar.drift_speed * 2.0;

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
        overall_short_padding: pir_short_padding,
        long_responses:        [],
        long_padding:          1.5 * wc.ms,
    },
};
local pirs = [pir(p) for p in [0, 1, 2]];

// ---------------------------------------------------------------------------
// Drifter: uses this anode's faces as the drift xregions
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

// ---------------------------------------------------------------------------
// DepoTransform (IDepoSet → IFrame)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Reframer: crops field-response headroom; forces tbin=0 on output traces.
// OmnibusSigProc requires tbin=0 on all input traces.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Noise model + AddNoise
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Digitizer: floating-point voltage traces → integer ADC counts
// ---------------------------------------------------------------------------

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
// Boundary nodes
// ---------------------------------------------------------------------------

local src = sources[0] { data: {} };

local snk = sinks[0] { data: {} };

// ---------------------------------------------------------------------------
// Full component list + Pgrapher
// ---------------------------------------------------------------------------

[dft, rng, wires, fr, elec, anode] + pirs +
[drifter_comp, setdrifter, transform, reframer, noise_model, addnoise, digitizer, src, snk,
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
              head: { node: wc.tn(snk),          port: 0 } },
        ],
    },
}]
