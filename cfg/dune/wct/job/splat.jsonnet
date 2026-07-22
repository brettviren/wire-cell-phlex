// cfg/dune/wct/job/splat.jsonnet
//
// WCT sub-graph: GenericDepoSetBoundarySource → DepoSetDrifter → DepoFluxSplat
//                → Reframer → GenericFrameBoundarySink
//
// Implements drift + DepoFluxSplat ("true signal") for a single anode of any
// DUNE detector described by a DetectorDescription object (see wct/dets/).
// The output frame represents the "truth" for comparison against sim+sigproc
// in the SPDIR workflow.
//
// The detector is selected by the `detector` TLA (canonical name string).  The
// per-anode configuration is taken from det.anodes[anode_index].
//
// TLA parameters (same signature as sim.jsonnet):
//   source_name_0    (string): instance name for GenericDepoSetBoundarySource
//   sink_name_0      (string): instance name for GenericFrameBoundarySink
//   app_name       (string): instance name for Pgrapher
//   detector       (string): canonical detector name, e.g. "pdhd" or "pdvd"
//   anode_index    (string): anode index into det.anodes[], e.g. "0"
//   service_prefix (string): prefix for WCT service component names
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellAux

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

// ---------------------------------------------------------------------------
// Derived timing quantities
// ---------------------------------------------------------------------------
local tick            = det.daq.tick;
local nticks_daq      = det.daq.nticks;
local response_nticks = wc.roundToInt(det.response_plane / det.lar.drift_speed / tick);
local nticks_ductor   = nticks_daq + response_nticks;

// ---------------------------------------------------------------------------
// Service components (shared subset of sim.jsonnet — no elec/pirs/noise needed)
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

// ---------------------------------------------------------------------------
// Wire geometry
// ---------------------------------------------------------------------------

local wires = {
    type: "WireSchemaFile",
    name: service_prefix + "wires_" + a.name,
    data: { filename: det.wires.filename },
};

// ---------------------------------------------------------------------------
// Field response (DepoFluxSplat uses it for origin and drift speed only)
// ---------------------------------------------------------------------------

local fr = {
    type: "FieldResponse",
    name: service_prefix + "fr_" + a.name,
    data: { filename: a.field.filename },
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
// Drifter
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
// DepoFluxSplat: maps drifted depos → signal frame (truth reference).
// Uses det.splat for all tuning (tick, window, smear values).
// ---------------------------------------------------------------------------

local splat = {
    type: "DepoFluxSplat",
    name: service_prefix + "splat_" + a.name,
    data: det.splat + {
        anode:          wc.tn(anode),
        field_response: wc.tn(fr),   // only period and origin are used
    },
};

// ---------------------------------------------------------------------------
// Reframer: same as in sim.jsonnet — crops response headroom, forces tbin=0.
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

[dft, rng, wires, fr, anode, drifter_comp, setdrifter, splat, reframer, src, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            { tail: { node: wc.tn(src),        port: 0 },
              head: { node: wc.tn(setdrifter),  port: 0 } },
            { tail: { node: wc.tn(setdrifter),  port: 0 },
              head: { node: wc.tn(splat),        port: 0 } },
            { tail: { node: wc.tn(splat),        port: 0 },
              head: { node: wc.tn(reframer),    port: 0 } },
            { tail: { node: wc.tn(reframer),    port: 0 },
              head: { node: wc.tn(snk),          port: 0 } },
        ],
    },
}]
