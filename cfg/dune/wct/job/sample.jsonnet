// cfg/dune/wct/job/sample.jsonnet
//
// WCT sub-graph: TrackSegmentSetBoundarySource → TrackSegmentSampler
//                → DepoSetBoundarySink
//   (or, with input="deposet": DepoSetBoundarySource → DepoSetBoundarySink, a
//    pass-through — normally use input="tracksegmentset")
//
// This is the front slice of drift.jsonnet stopped BEFORE the drifter: the
// UNDRIFTED ("initial") depos as sampled from the energy-deposit track segments,
// i.e. the input the DepoSetDrifter would consume.  It exists so a full-chain
// job can tap and persist the initial depos alongside the post-drift depos, the
// ADC frame and the signal-processed frame (xerosere multi-event test), without
// perturbing the sim island.
//
// No rng is built: TrackSegmentSampler's recombination + fixed step size are
// deterministic, so the sampled depos are reproducible with no random draws.
//
// The detector is selected by the `detector` TLA (canonical name string).  The
// per-anode configuration is taken from det.anodes[anode_index]; only the
// anode's lar/sim parameters (efield for recombination) are used — no wire
// geometry, drift, field response, electronics or noise components are built.
//
// TLA parameters (same signature as drift.jsonnet):
//   sources  — array of WCT inode { type, name } for the boundary source(s)
//   sinks    — array of WCT inode { type, name } for the boundary sink(s)
//   app_name       (string): instance name for Pgrapher
//   detector       (string): canonical detector name, e.g. "pdhd" or "pdvd"
//   anode_index    (string): anode index into det.anodes[], e.g. "0"
//   service_prefix (string): prefix for all WCT service component names
//   input          (string): "tracksegmentset" (prepend TrackSegmentSampler)
//                            or "deposet" (source feeds the sink directly)
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellAux

local wc   = import "wirecell.jsonnet";
local dets = import "dune/wct/dets.jsonnet";

function(
    sources  = [],
    sinks    = [],
    app_name       = "wcphlex_pgrapher",
    detector       = "pdhd",
    anode_index    = "0",
    service_prefix = "",
    input          = "tracksegmentset",  // or "deposet": source feeds the sink directly
)

local det = dets[detector]({detname: detector});
local ai  = std.parseInt(anode_index);
local a   = det.anodes[ai];

// ---------------------------------------------------------------------------
// Boundary nodes
// ---------------------------------------------------------------------------

local src = sources[0] { data: {} };

local snk = sinks[0] { data: {} };

// ---------------------------------------------------------------------------
// Front end: energy-deposit segments -> depos (segment sampler), identical to
// drift.jsonnet / sim.jsonnet so the sampled depos are exactly those the
// drifter downstream would receive.
// ---------------------------------------------------------------------------

local segments = input == "tracksegmentset";

local recomb = {
    type: "BoxRecombination",
    name: service_prefix + "recomb_" + a.name,
    data: { Efield: std.get(det.lar, "efield", 500 * wc.volt / wc.cm) },
};

local sampler = {
    type: "TrackSegmentSampler",
    name: service_prefix + "sampler_" + a.name,
    data: {
        ionization: "recombination",
        recombination: wc.tn(recomb),
        step_size: 1.0 * wc.mm,
    },
};

// Edges from the boundary source to the sink, with or without the sampler.
local edges = if segments then [
    { tail: { node: wc.tn(src),     port: 0 },
      head: { node: wc.tn(sampler), port: 0 } },
    { tail: { node: wc.tn(sampler), port: 0 },
      head: { node: wc.tn(snk),     port: 0 } },
] else [
    { tail: { node: wc.tn(src), port: 0 },
      head: { node: wc.tn(snk), port: 0 } },
];

// ---------------------------------------------------------------------------
// Full component list + Pgrapher
// ---------------------------------------------------------------------------

[src, snk] +
(if segments then [recomb, sampler] else []) +
[
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: edges,
    },
}]
