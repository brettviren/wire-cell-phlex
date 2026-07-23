// cfg/deposet-file-source.jsonnet
//
// WCT sub-graph: DepoFileSource → DepoSet boundary sink.
//
// The IDepoSet source shape (SourceExecutor<IDepoSet>): the WCT graph is run
// once to completion; all depo sets read from the file queue in the boundary
// sink, and PHLEX drains one per event.
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array (empty: the origin is the real DepoFileSource, not a boundary)
//   sinks    — array of WCT inode objects { type, name }: one DepoSet boundary sink
//   app_name — instance name for the Pgrapher application
//   inname   — input file path (passed via wct_tla from the module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
    inname   = "depos.npz",
)

local snk = sinks[0];

[
    // WCT file source: reads depo sets from disk.
    {
        type: "DepoFileSource",
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
                tail: { node: "DepoFileSource:file_source", port: 0 },
                head: { node: snk.type + ":" + snk.name,    port: 0 },
            },
        ]},
    },
]
