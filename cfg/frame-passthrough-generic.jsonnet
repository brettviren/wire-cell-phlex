// cfg/frame-passthrough-generic.jsonnet
//
// Identity IFrame → IFrame passthrough: a Frame boundary source wired straight
// to a Frame boundary sink.  Used by test_function_executor to prove
// FunctionExecutor<IFrame,IFrame> wires through Pgraph, and by the plain
// wcph_frame_filter path.
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array of WCT inode objects { type, name }: one Frame boundary source
//   sinks    — array of WCT inode objects { type, name }: one Frame boundary sink
//   app_name — instance name for the Pgrapher application

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
)

local src = sources[0];
local snk = sinks[0];

[
    src { data: {} },
    snk { data: {} },
    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: src.type + ":" + src.name, port: 0 },
                head: { node: snk.type + ":" + snk.name, port: 0 },
            },
        ]},
    },
]
