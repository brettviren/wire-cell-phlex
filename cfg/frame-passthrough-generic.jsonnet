// cfg/frame-passthrough-generic.jsonnet
//
// Like frame-passthrough.jsonnet, but using the GenericFrameBoundarySource /
// GenericFrameBoundarySink types — the boundary nodes whose WCT interface is
// the templated ISourceNode<IFrame> / ISinkNode<IFrame> rather than the
// concrete IFrameSource / IFrameSink.  Used by test_function_executor to prove
// FunctionExecutor<IFrame,IFrame> wires through Pgraph.
//
// TLA parameters (indexed per port, bound by PortedExecutor):
//   source_name_0, sink_name_0, app_name

function(
    source_name_0 = "wcphlex_source_0",
    sink_name_0   = "wcphlex_sink_0",
    app_name      = "wcphlex_pgrapher",
)
[
    { type: "GenericFrameBoundarySource", name: source_name_0, data: {} },
    { type: "GenericFrameBoundarySink",   name: sink_name_0,   data: {} },
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: "GenericFrameBoundarySource:" + source_name_0, port: 0 },
                    head: { node: "GenericFrameBoundarySink:"   + sink_name_0,   port: 0 },
                },
            ],
        },
    },
]
