// cfg/frame-file-source.jsonnet
//
// WCT sub-graph: FrameFileSource → Frame boundary sink.
//
// The IFrame source shape (SourceExecutor<IFrame>): the WCT graph is run once
// to completion; all frames read from the file queue in the boundary sink, and
// PHLEX drains one per event.
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array (empty: the origin is the real FrameFileSource, not a boundary)
//   sinks    — array of WCT inode objects { type, name }: one Frame boundary sink
//   app_name — instance name for the Pgrapher application
//   inname   — input file path (passed via wct_tla from the module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
    inname   = "frames.npz",
)

local snk = sinks[0];

[
    // WCT file source: reads frames from disk (WCT "frame file" format).
    {
        type: "FrameFileSource",
        name: "file_source",
        data: {
            inname: inname,
        },
    },

    snk { data: {} },

    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: "FrameFileSource:file_source", port: 0 },
                head: { node: snk.type + ":" + snk.name,      port: 0 },
            },
        ]},
    },
]
