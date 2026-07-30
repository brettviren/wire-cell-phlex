// cfg/tracksegmentset-sampler.jsonnet
//
// WCT configuration: TrackSegmentSet boundary source → Gen::TrackSegmentSampler
// → DepoSet boundary sink.  Converts energy-deposit segments to point depos
// with an ionization model (WCT sub-graph for wcph_tracksegmentset_to_deposet).
//
// The recombination model is its own named WCT component; its E-field etc. are
// configured here (per the segment-sampler ADR all sampler physics parameters
// flow through WCT Jsonnet).
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — one TrackSegmentSet boundary source { type, name }
//   sinks    — one DepoSet boundary sink { type, name }
//   app_name — instance name for the Pgrapher application
// Optional TLAs (strings, as all executor wct_tla values are):
//   ionization — "recombination" (default) or "quanta"
//   step_size  — sample spacing in mm (default "1.0")
//   efield     — drift field in V/cm for the recombination model (default "500")

function(
    sources  = [],
    sinks    = [],
    app_name = "wcph_pgrapher",
    ionization = "recombination",
    step_size = "1.0",
    efield = "500",
)

local wc = import "wirecell.jsonnet";

local src = sources[0];
local snk = sinks[0];

local recomb = {
    type: "BoxRecombination",
    name: "",
    data: {
        Efield: std.parseJson(efield) * wc.volt / wc.cm,
    },
};

local sampler = {
    type: "TrackSegmentSampler",
    name: "",
    data: {
        ionization: ionization,
        recombination: wc.tn(recomb),
        step_size: std.parseJson(step_size) * wc.mm,
    },
};

[
    src { data: {} },
    recomb,
    sampler,
    snk { data: {} },
    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: src.type + ":" + src.name, port: 0 },
                head: { node: wc.tn(sampler), port: 0 },
            },
            {
                tail: { node: wc.tn(sampler), port: 0 },
                head: { node: snk.type + ":" + snk.name, port: 0 },
            },
        ]},
    },
]
