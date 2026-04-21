// cfg/frame-file-sink.jsonnet
//
// WCT sub-graph: FrameBoundarySource → FrameFileSink.
//
// Used by the FrameSinkFile executor.  Each PHLEX event fills the boundary
// source with one Frame and runs the graph once.  The FrameFileSink (ITerminal)
// accumulates all events; WireCell::Main::~Main() calls finalize() to flush
// and close the output file.
//
// TLA parameters:
//   source_name (string): instance name for the FrameBoundarySource node
//   app_name    (string): instance name for the Pgrapher application
//   outname     (string): output file path (passed via wct_tla from module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    source_name = "wcphlex_frame_source",
    app_name    = "wcphlex_pgrapher",
    outname     = "frames-out.npz",
)
[
    // Boundary source: FrameSinkFile executor fills this per PHLEX event.
    {
        type: "FrameBoundarySource",
        name: source_name,
        data: {},
    },

    // WCT file sink: writes frames to disk in "frame file" format.  finalize()
    // closes the stream when WireCell::Main is destroyed.
    {
        type: "FrameFileSink",
        name: "file_sink",
        data: {
            outname: outname,
            tags: ["*"],       // write all trace tags
            digitize: false,   // float output (not int16)
        },
    },

    // Pgrapher: wire boundary source → file sink.
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: "FrameBoundarySource:" + source_name, port: 0 },
                    head: { node: "FrameFileSink:file_sink", port: 0 },
                },
            ],
        },
    },
]
