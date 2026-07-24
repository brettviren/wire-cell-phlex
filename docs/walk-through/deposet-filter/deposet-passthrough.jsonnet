// deposet-passthrough.jsonnet
//
// Minimal WCT configuration: DepoSet boundary source → DepoSet boundary sink
// with no processing in between — the identity IDepoSet → IDepoSet sub-graph
// wcph_deposet_filter runs on each event.
//
// TLA parameters (injected by the ShapeExecutor base as code-valued arrays):
//   sources  — array of WCT inode objects { type, name }: one DepoSet boundary source
//   sinks    — array of WCT inode objects { type, name }: one DepoSet boundary sink
//   app_name — instance name for the Pgrapher application
//
// The boundary node types (DepoSetBoundarySource/Sink) arrive inside the
// sources/sinks arrays, so this config never hard-codes them.

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
)

local src = sources[0];
local snk = sinks[0];

[
    // Boundary source: PHLEX fills this before each WCT run.
    src { data: {} },

    // Boundary sink: PHLEX drains this after each WCT run.
    snk { data: {} },

    // Pgrapher: wire source → sink.
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
