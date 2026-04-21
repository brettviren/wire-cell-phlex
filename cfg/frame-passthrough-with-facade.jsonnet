// cfg/frame-passthrough-with-facade.jsonnet
//
// WCT configuration: FrameBoundarySource → FrameBoundarySink with a
// FacadeWireSchema service and a WireSchemaValidator.
//
// This variant of frame-passthrough.jsonnet adds geometry awareness:
//   - FacadeWireSchema: implements IWireSchema by reading from the static
//     registry populated by the Executor before initialize() is called.
//   - WireSchemaValidator: an IConfigurable that looks up the FacadeWireSchema
//     service and asserts it contains at least one wire.  Used to verify that
//     geometry was correctly bridged from PHLEX into the WCT configure phase.
//
// TLA parameters:
//   source_name      (string): instance name for FrameBoundarySource
//   sink_name        (string): instance name for FrameBoundarySink
//   app_name         (string): instance name for the Pgrapher application
//   wire_schema_name (string): instance name for FacadeWireSchema (and the
//                    "scope" key in its data — must match the executor's scope)

function(
    source_name      = "wcphlex_frame_source",
    sink_name        = "wcphlex_frame_sink",
    app_name         = "wcphlex_pgrapher",
    wire_schema_name = "wcphlex",
)
[
    // FacadeWireSchema: bridges PHLEX job-layer geometry into WCT configure time.
    // "scope" must match wire_schema_name so FacadeWireSchema::configure() can
    // look up the pre-registered WireSchema::Store in the static map.
    {
        type: "FacadeWireSchema",
        name: wire_schema_name,
        data: {
            scope: wire_schema_name,
        },
    },

    // WireSchemaValidator: verifies FacadeWireSchema is findable and non-empty.
    // Runs during configure(); will throw if the store was not pre-registered.
    {
        type: "WireSchemaValidator",
        name: wire_schema_name,
        data: {
            wire_schema: "FacadeWireSchema:" + wire_schema_name,
        },
    },

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
