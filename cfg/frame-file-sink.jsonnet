// cfg/frame-file-sink.jsonnet
//
// WCT sub-graph: Frame boundary source → FrameFileSink.
//
// The IFrame sink shape (SinkExecutor<IFrame>): each PHLEX event fills the
// boundary source with one Frame and runs the graph once.  The FrameFileSink
// (ITerminal) accumulates all events; WireCell::Main::~Main() calls finalize()
// to flush and close the output file.
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array of WCT inode objects { type, name }: one Frame boundary source
//   sinks    — array (empty: the terminal is the real FrameFileSink, not a boundary)
//   app_name — instance name for the Pgrapher application
//   outname  — output file path (passed via wct_tla from the module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
    outname  = "frames-out.npz",
)

local src = sources[0];

[
    src { data: {} },

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

    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: src.type + ":" + src.name, port: 0 },
                head: { node: "FrameFileSink:file_sink", port: 0 },
            },
        ]},
    },
]
