// cfg/deposet-file-sink.jsonnet
//
// WCT sub-graph: DepoSet boundary source → DepoFileSink.
//
// The IDepoSet sink shape (SinkExecutor<IDepoSet>): each PHLEX event fills the
// boundary source with one DepoSet and runs the graph once.  The DepoFileSink
// (ITerminal) accumulates all events; WireCell::Main::~Main() calls finalize()
// to flush and close the output file.
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array of WCT inode objects { type, name }: one DepoSet boundary source
//   sinks    — array (empty: the terminal is the real DepoFileSink, not a boundary)
//   app_name — instance name for the Pgrapher application
//   outname  — output file path (passed via wct_tla from the module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
    outname  = "depos-out.npz",
)

local src = sources[0];

[
    src { data: {} },

    // WCT file sink: writes depo sets to disk.  finalize() closes the stream.
    {
        type: "DepoFileSink",
        name: "file_sink",
        data: {
            outname: outname,
        },
    },

    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: src.type + ":" + src.name, port: 0 },
                head: { node: "DepoFileSink:file_sink",  port: 0 },
            },
        ]},
    },
]
