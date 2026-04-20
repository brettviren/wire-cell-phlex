// cfg/frame-passthrough.jsonnet
//
// Minimal WCT configuration: FrameBoundarySource → FrameBoundarySink with no
// processing in between.  Used as a smoke test for the Executor class.
//
// This file is a Jsonnet function so it can receive instance names as
// top-level arguments (TLAs) from the Executor.  Default values are provided
// for stand-alone manual testing via `jsonnet --tla-str`.
//
// TLA parameters:
//   source_name (string): instance name for the FrameBoundarySource node
//   sink_name   (string): instance name for the FrameBoundarySink node
//   app_name    (string): instance name for the Pgrapher application

function(
    source_name = "wcphlex_frame_source",
    sink_name   = "wcphlex_frame_sink",
    app_name    = "wcphlex_pgrapher",
)
[
    // Boundary source: PHLEX fills this before each WCT run.
    {
        type: "FrameBoundarySource",
        name: source_name,
        data: {},
    },

    // Boundary sink: PHLEX drains this after each WCT run.
    {
        type: "FrameBoundarySink",
        name: sink_name,
        data: {},
    },

    // Pgrapher: wire source → sink.
    // Node references use the "Type:name" format required by Pgrapher::configure().
    {
        type: "Pgrapher",
        name: app_name,
        data: {
            edges: [
                {
                    tail: { node: "FrameBoundarySource:" + source_name, port: 0 },
                    head: { node: "FrameBoundarySink:"   + sink_name,   port: 0 },
                },
            ],
        },
    },
]
