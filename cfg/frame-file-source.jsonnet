// cfg/frame-file-source.jsonnet
//
// WCT sub-graph: FrameFileSource → FrameBoundarySink.
//
// Used by the FrameSourceFile executor.  The WCT graph is run once to
// completion; all frames read from the file queue in the boundary sink.
//
// TLA parameters:
//   sink_name  (string): instance name for the FrameBoundarySink node
//   app_name   (string): instance name for the Pgrapher application
//   inname     (string): input file path (passed via wct_tla from module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    sink_name = "wcphlex_frame_sink",
    app_name  = "wcphlex_pgrapher",
    inname    = "frames.npz",
)
[
    // WCT file source: reads frames from disk (WCT "frame file" format).
    {
        type: "FrameFileSource",
        name: "file_source",
        data: {
            inname: inname,
        },
    },

    // Boundary sink: FrameSourceFile executor drains this after graph run.
    {
        type: "FrameBoundarySink",
        name: sink_name,
        data: {},
    },

    // Pgrapher: wire file source → boundary sink.
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: "FrameFileSource:file_source", port: 0 },
                    head: { node: "FrameBoundarySink:" + sink_name, port: 0 },
                },
            ],
        },
    },
]
