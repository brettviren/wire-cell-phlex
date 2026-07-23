// cfg/deposet-drifter.jsonnet
//
// WCT sub-graph: DepoSetBoundarySource → DepoSetDrifter (Drifter+Random)
//                → DepoSetBoundarySink.
//
// Used by wcph_deposet_filter to run the WCT drift simulation on each PHLEX event.
//
// Physics parameters use a fictional single-APA geometry for testing.
// Set fluctuate: false for determinism in unit tests.
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array of WCT inode { type, name } for the boundary source(s)
//   sinks    — array of WCT inode { type, name } for the boundary sink(s)
//   app_name — instance name for the Pgrapher application
//
// Required WCT plugins: WireCellPgraph, WireCellGen

local wc = import "wirecell.jsonnet";

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
)

local src = sources[0];
local snk = sinks[0];

[
    // Random number generator (required by Drifter).
    {
        type: "Random",
        name: "rng",
        data: {},
    },

    // Per-depo drifter: handles diffusion, lifetime, and X-region boundaries.
    {
        type: "Drifter",
        name: "drifter",
        data: {
            rng: "Random:rng",
            // Longitudinal and transverse diffusion coefficients.
            DL: 7.2 * wc.cm2 / wc.s,
            DT: 12.0 * wc.cm2 / wc.s,
            // Electron lifetime (argon purity).
            lifetime: 8 * wc.ms,
            // Nominal drift speed for liquid argon.
            drift_speed: 1.6 * wc.mm / wc.us,
            // Disable Poisson fluctuations for deterministic test output.
            fluctuate: false,
            // Fictional single-APA x-region: anode at x=0, cathode at x=3.6m.
            xregions: [
                {
                    anode:    0.0,
                    response: 10 * wc.cm,
                    cathode:  3.6 * wc.m,
                },
            ],
        },
    },

    // DepoSet-level wrapper around Drifter: applies per-depo drift to each set.
    {
        type: "DepoSetDrifter",
        name: "deposet_drifter",
        data: {
            drifter: "Drifter:drifter",
        },
    },

    // Boundary source: DepoSetFilter executor fills this before each WCT run.
    src { data: {} },

    // Boundary sink: DepoSetFilter executor drains this after each WCT run.
    snk { data: {} },

    // Pgrapher: boundary source → drifter → boundary sink.
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: src.type + ":" + src.name,        port: 0 },
                    head: { node: "DepoSetDrifter:deposet_drifter", port: 0 },
                },
                {
                    tail: { node: "DepoSetDrifter:deposet_drifter", port: 0 },
                    head: { node: snk.type + ":" + snk.name,        port: 0 },
                },
            ],
        },
    },
]
