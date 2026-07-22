// cfg/deposet-passthrough-generic.jsonnet
//
// Minimal WCT configuration: GenericDepoSetBoundarySource → GenericDepoSetBoundarySink
// with no processing in between.  Identity IDepoSet → IDepoSet smoke test for
// the templated FunctionExecutor<IDepoSet,IDepoSet> (wcph_deposet_filter).
//
// (The non-generic deposet-passthrough.jsonnet is retained for the C++ unit
// test test_executor, which drives the original DepoSetFilter executor.)
//
// TLA parameters (injected by FunctionExecutor<IDepoSet,IDepoSet> — indexed per
// port; single-port shapes use index 0):
//   source_name_0 (string): instance name for GenericDepoSetBoundarySource
//   sink_name_0   (string): instance name for GenericDepoSetBoundarySink
//   app_name      (string): instance name for the Pgrapher application

function(
    source_name_0 = "wcph_deposet_source",
    sink_name_0   = "wcph_deposet_sink",
    app_name      = "wcph_pgrapher",
)
[
    { type: "GenericDepoSetBoundarySource", name: source_name_0, data: {} },
    { type: "GenericDepoSetBoundarySink",   name: sink_name_0,   data: {} },
    {
        type: "Pgrapher",
        name: app_name,
        data: { edges: [
            {
                tail: { node: "GenericDepoSetBoundarySource:" + source_name_0, port: 0 },
                head: { node: "GenericDepoSetBoundarySink:"   + sink_name_0,   port: 0 },
            },
        ]},
    },
]
