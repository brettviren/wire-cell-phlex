// cfg/deposet-passthrough.jsonnet
//
// Minimal WCT configuration: DepoSetBoundarySource → DepoSetBoundarySink with
// no processing in between.  Used as a smoke test for the DepoSetFilter executor.
//
// TLA parameters:
//   source_name (string): instance name for the DepoSetBoundarySource node
//   sink_name   (string): instance name for the DepoSetBoundarySink node
//   app_name    (string): instance name for the Pgrapher application

function(
    source_name = "wcphlex_deposet_source",
    sink_name   = "wcphlex_deposet_sink",
    app_name    = "wcphlex_pgrapher",
)
[
    // Boundary source: PHLEX fills this before each WCT run.
    {
        type: "DepoSetBoundarySource",
        name: source_name,
        data: {},
    },

    // Boundary sink: PHLEX drains this after each WCT run.
    {
        type: "DepoSetBoundarySink",
        name: sink_name,
        data: {},
    },

    // Pgrapher: wire source → sink.
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: "DepoSetBoundarySource:" + source_name, port: 0 },
                    head: { node: "DepoSetBoundarySink:"   + sink_name,   port: 0 },
                },
            ],
        },
    },
]
