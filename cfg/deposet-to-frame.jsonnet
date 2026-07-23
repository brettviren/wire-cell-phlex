// cfg/deposet-to-frame.jsonnet
//
// Minimal WCT graph for testing wcphlex::DepoSetToFrame:
//
//   DepoSet boundary source ──[IDepoSet]──> TrivialDepoFramer ──[IFrame]──> Frame boundary sink
//
// TrivialDepoFramer produces an empty IFrame whose ident matches the
// incoming IDepoSet ident.  This is a connectivity test only.
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array of WCT inode objects { type, name } for the boundary
//              sources this shape needs (here: one DepoSet boundary source)
//   sinks    — array of WCT inode objects { type, name } for the boundary
//              sinks (here: one Frame boundary sink)
//   app_name — instance name for the Pgrapher

function(
    sources  = [],
    sinks    = [],
    app_name = "wcph_pgrapher",
)

local src = sources[0];   // DepoSet boundary source inode { type, name }
local snk = sinks[0];      // Frame boundary sink inode { type, name }

[
    src { data: {} },
    snk { data: {} },
    { type: "TrivialDepoFramer", name: "converter", data: {} },
    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: src.type + ":" + src.name,     port: 0 },
                head: { node: "TrivialDepoFramer:converter", port: 0 },
            },
            {
                tail: { node: "TrivialDepoFramer:converter", port: 0 },
                head: { node: snk.type + ":" + snk.name,     port: 0 },
            },
        ]},
    },
]
