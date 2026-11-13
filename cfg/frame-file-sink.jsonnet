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
//   tags     — trace tags to write ("*" default; see below)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
    outname  = "frames-out.npz",
    // Trace tags to write, comma separated (a Phlex TLA is a string) or an
    // array.  "*" writes all traces of a frame as one array, summing traces
    // of different tags on the same channel: give the tags of a frame that
    // carries several (e.g. "gauss,wiener" for signal processing output).
    tags     = "*",
)

local tag_list = if std.isArray(tags) then tags else std.split(tags, ",");

local src = sources[0];

// The sink's name follows the (per-module) app name: WCT components are
// process-wide by type:name, so two sink modules must not share one.
local sink_name = app_name + "_file_sink";

[
    src { data: {} },

    // WCT file sink: writes frames to disk in "frame file" format.  finalize()
    // closes the stream when WireCell::Main is destroyed.
    {
        type: "FrameFileSink",
        name: sink_name,
        data: {
            outname: outname,
            tags: tag_list,
            digitize: false,   // float output (not int16)
        },
    },

    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: src.type + ":" + src.name, port: 0 },
                head: { node: "FrameFileSink:" + sink_name, port: 0 },
            },
        ]},
    },
]
