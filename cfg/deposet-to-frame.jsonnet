// cfg/deposet-to-frame.jsonnet
//
// Minimal WCT graph for testing wcphlex::DepoSetToFrame:
//
//   DepoSetBoundarySource ──[IDepoSet]──> TrivialDepoFramer ──[IFrame]──> FrameBoundarySink
//
// TrivialDepoFramer produces an empty IFrame whose ident matches the
// incoming IDepoSet ident.  This is a connectivity test only.
//
// TLA parameters (injected by FunctionExecutor<IDepoSet,IFrame> — indexed per
// port; single-port shapes use index 0):
//   source_name_0 — instance name for the (generic) DepoSet boundary source
//   sink_name_0   — instance name for the (generic) Frame boundary sink
//   app_name      — instance name for the Pgrapher

function(
    source_name_0 = "wcph_deposet_source",
    sink_name_0   = "wcph_frame_sink",
    app_name      = "wcph_pgrapher",
)
[
    { type: "GenericDepoSetBoundarySource", name: source_name_0, data: {} },
    { type: "GenericFrameBoundarySink",     name: sink_name_0,   data: {} },
    { type: "TrivialDepoFramer",            name: "converter",   data: {} },
    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: "GenericDepoSetBoundarySource:" + source_name_0, port: 0 },
                head: { node: "TrivialDepoFramer:converter",                   port: 0 },
            },
            {
                tail: { node: "TrivialDepoFramer:converter",                   port: 0 },
                head: { node: "GenericFrameBoundarySink:" + sink_name_0,       port: 0 },
            },
        ]},
    },
]
