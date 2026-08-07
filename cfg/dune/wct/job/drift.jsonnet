// cfg/dune/wct/job/drift.jsonnet
//
// WCT sub-graph: TrackSegmentSetBoundarySource → TrackSegmentSampler
//                → DepoSetDrifter → DepoSetBoundarySink
//   (or, with input="deposet": DepoSetBoundarySource → DepoSetDrifter → sink)
//
// This is the front slice of sim.jsonnet stopped at the post-drift depos: the
// depos that DepoTransform would consume to make the ADC frame.  It exists so a
// full-chain job can tap and persist the drifted depos alongside the ADC and
// signal-processed frames (xerosere ddm-0hk) without re-running or perturbing
// the sim island.
//
// Reproducibility note: drift diffusion is fluctuated (det.sim.fluctuate), so
// the drifted depos depend on the rng draw sequence.  This config's rng is the
// drifter's ONLY consumer, so a freshly-seeded rng here (via a distinct
// service_prefix) yields the SAME first draws — hence the SAME drifted depos —
// as sim.jsonnet's own freshly-seeded rng, whose drifter also draws first.  Run
// the drift island with a service_prefix that does NOT collide with the sim
// island's bare names so each gets its own fresh, independently-reproducible rng.
//
// The detector is selected by the `detector` TLA (canonical name string).  The
// per-anode configuration is taken from det.anodes[anode_index]; only the
// anode's faces (drift xregions) and lar/sim parameters are used — no wire
// geometry, field response, electronics or noise components are built.
//
// TLA parameters (same signature as sim.jsonnet):
//   sources  — array of WCT inode { type, name } for the boundary source(s)
//   sinks    — array of WCT inode { type, name } for the boundary sink(s)
//   app_name       (string): instance name for Pgrapher
//   detector       (string): canonical detector name, e.g. "pdhd" or "pdvd"
//   anode_index    (string): anode index into det.anodes[], e.g. "0"
//   service_prefix (string): prefix for all WCT service component names
//   input          (string): "tracksegmentset" (prepend TrackSegmentSampler)
//                            or "deposet" (source feeds the drifter directly)
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
    input          = "deposet",  // or "tracksegmentset": prepend TrackSegmentSampler
)

local det = dets[detector]({detname: detector});
local ai  = std.parseInt(anode_index);
local a   = det.anodes[ai];

// ---------------------------------------------------------------------------
// Random service — the drifter's only rng consumer (see reproducibility note).
// ---------------------------------------------------------------------------

local rng = {
    type: "Random",
    name: service_prefix + "rng_" + a.name,
    data: { seed: 1 },   // fixed seed; matches sim.jsonnet's fresh rng draws
};

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
// Boundary nodes
// ---------------------------------------------------------------------------

local src = sources[0] { data: {} };

local snk = sinks[0] { data: {} };

// ---------------------------------------------------------------------------
// Optional front end: energy-deposit segments -> depos (segment sampler)
// (identical to sim.jsonnet so the drifter sees the SAME pre-drift depos)
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

// Edges from the boundary source to the drifter, with or without the sampler.
local front_edges = if segments then [
    { tail: { node: wc.tn(src),     port: 0 },
      head: { node: wc.tn(sampler), port: 0 } },
    { tail: { node: wc.tn(sampler), port: 0 },
      head: { node: wc.tn(setdrifter), port: 0 } },
] else [
    { tail: { node: wc.tn(src),        port: 0 },
      head: { node: wc.tn(setdrifter),  port: 0 } },
];

// ---------------------------------------------------------------------------
// Full component list + Pgrapher
// ---------------------------------------------------------------------------

[rng, drifter_comp, setdrifter, src, snk] +
(if segments then [recomb, sampler] else []) +
[
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: front_edges + [
            { tail: { node: wc.tn(setdrifter),  port: 0 },
              head: { node: wc.tn(snk),          port: 0 } },
        ],
    },
}]
