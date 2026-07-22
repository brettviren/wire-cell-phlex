// cfg/deposet-file-source.jsonnet
//
// WCT sub-graph: DepoFileSource → GenericDepoSetBoundarySink.
//
// Used by the DepoSetSourceFile executor.  The WCT graph is run once to
// completion; all depo sets read from the file queue in the boundary sink.
//
// TLA parameters:
//   sink_name_0  (string): instance name for the GenericDepoSetBoundarySink node
//   app_name   (string): instance name for the Pgrapher application
//   inname     (string): input file path (passed via wct_tla from module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    sink_name_0 = "wcphlex_deposet_sink",
    app_name  = "wcphlex_pgrapher",
    inname    = "depos.npz",
)
[
    // WCT file source: reads depo sets from disk.
    {
        type: "DepoFileSource",
        name: "file_source",
        data: {
            inname: inname,
        },
    },

    // Boundary sink: DepoSetSourceFile executor drains this after graph run.
    {
        type: "GenericDepoSetBoundarySink",
        name: sink_name_0,
        data: {},
    },

    // Pgrapher: wire file source → boundary sink.
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: "DepoFileSource:file_source", port: 0 },
                    head: { node: "GenericDepoSetBoundarySink:" + sink_name_0, port: 0 },
                },
            ],
        },
    },
]
