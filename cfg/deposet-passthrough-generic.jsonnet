// cfg/deposet-passthrough-generic.jsonnet
//
// Minimal WCT configuration: DepoSet boundary source → DepoSet boundary sink
// with no processing in between.  Identity IDepoSet → IDepoSet smoke test for
// the templated FunctionExecutor<IDepoSet,IDepoSet> (wcph_deposet_filter).
//
// (The non-generic deposet-passthrough.jsonnet is retained for the C++ unit
// test test_executor, which drives the original DepoSetFilter executor.)
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array of WCT inode objects { type, name }: one DepoSet boundary source
//   sinks    — array of WCT inode objects { type, name }: one DepoSet boundary sink
//   app_name — instance name for the Pgrapher application

function(
    sources  = [],
    sinks    = [],
    app_name = "wcph_pgrapher",
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
