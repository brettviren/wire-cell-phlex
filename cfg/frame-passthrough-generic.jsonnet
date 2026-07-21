// cfg/frame-passthrough-generic.jsonnet
//
// Like frame-passthrough.jsonnet, but using the GenericFrameBoundarySource /
// GenericFrameBoundarySink types — the boundary nodes whose WCT interface is
// the templated ISourceNode<IFrame> / ISinkNode<IFrame> rather than the
// concrete IFrameSource / IFrameSink.  Used by test_function_executor to prove
// FunctionExecutor<IFrame,IFrame> wires through Pgraph.
//
// TLA parameters: source_name, sink_name, app_name (bound by the Executor).

function(
    source_name = "wcphlex_frame_source",
    sink_name   = "wcphlex_frame_sink",
    app_name    = "wcphlex_pgrapher",
)
[
    { type: "GenericFrameBoundarySource", name: source_name, data: {} },
    { type: "GenericFrameBoundarySink",   name: sink_name,   data: {} },
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: "GenericFrameBoundarySource:" + source_name, port: 0 },
                    head: { node: "GenericFrameBoundarySink:"   + sink_name,   port: 0 },
                },
            ],
        },
    },
]
