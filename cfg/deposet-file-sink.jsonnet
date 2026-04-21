// cfg/deposet-file-sink.jsonnet
//
// WCT sub-graph: DepoSetBoundarySource → DepoFileSink.
//
// Used by the DepoSetSinkFile executor.  Each PHLEX event fills the boundary
// source with one DepoSet and runs the graph once.  The DepoFileSink (ITerminal)
// accumulates all events; the executor's destructor calls finalize() to flush
// and close the output file.
//
// TLA parameters:
//   source_name (string): instance name for the DepoSetBoundarySource node
//   app_name    (string): instance name for the Pgrapher application
//   outname     (string): output file path (passed via wct_tla from module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    source_name = "wcphlex_deposet_source",
    app_name    = "wcphlex_pgrapher",
    outname     = "depos-out.npz",
)
[
    // Boundary source: DepoSetSinkFile executor fills this per PHLEX event.
    {
        type: "DepoSetBoundarySource",
        name: source_name,
        data: {},
    },

    // WCT file sink: writes depo sets to disk.  finalize() closes the stream.
    {
        type: "DepoFileSink",
        name: "file_sink",
        data: {
            outname: outname,
        },
    },

    // Pgrapher: wire boundary source → file sink.
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: "DepoSetBoundarySource:" + source_name, port: 0 },
                    head: { node: "DepoFileSink:file_sink", port: 0 },
                },
            ],
        },
    },
]
