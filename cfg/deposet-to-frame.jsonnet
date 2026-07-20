// cfg/deposet-to-frame.jsonnet
//
// Minimal WCT graph for testing wcphlex::DepoSetToFrame:
//
//   DepoSetBoundarySource ──[IDepoSet]──> TrivialDepoFramer ──[IFrame]──> FrameBoundarySink
//
// TrivialDepoFramer produces an empty IFrame whose ident matches the
// incoming IDepoSet ident.  This is a connectivity test only.
//
// TLA parameters (injected by wcphlex::DepoSetToFrame constructor):
//   source_name — instance name for DepoSetBoundarySource
//   sink_name   — instance name for FrameBoundarySink
//   app_name    — instance name for the Pgrapher

function(
    source_name = "wcph_deposet_source",
    sink_name   = "wcph_frame_sink",
    app_name    = "wcph_pgrapher",
)
[
    { type: "DepoSetBoundarySource", name: source_name, data: {} },
    { type: "FrameBoundarySink",     name: sink_name,   data: {} },
    { type: "TrivialDepoFramer",     name: "converter", data: {} },
    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: "DepoSetBoundarySource:" + source_name, port: 0 },
                head: { node: "TrivialDepoFramer:converter",          port: 0 },
            },
            {
                tail: { node: "TrivialDepoFramer:converter",          port: 0 },
                head: { node: "FrameBoundarySink:" + sink_name,       port: 0 },
            },
        ]},
    },
]
