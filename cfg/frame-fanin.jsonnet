// cfg/frame-fanin.jsonnet
//
// WCT sub-graph: 4×GenericFrameBoundarySource → FrameFanin → GenericFrameBoundarySink.
//
// The fanin-only shape: each PHLEX event delivers four Frames (one per APA) to
// the four boundary sources; FrameFanin merges them into a single Frame
// (concatenating traces, merging channel masks) which is drained back to PHLEX
// through the boundary sink.  (Writing the merged Frame to a file is a separate
// sink node — see frame-file-sink.jsonnet.)
//
// TLA parameters (injected by FaninExecutor<IFrame,IFrame> — indexed per port):
//   source_name_0..3 (string): instance names for the 4 GenericFrameBoundarySource nodes
//   sink_name_0      (string): instance name for the GenericFrameBoundarySink node
//   app_name         (string): instance name for the Pgrapher
//
// Required WCT plugins: WireCellPgraph, WireCellGen

local wc = import "wirecell.jsonnet";

function(
    source_name_0 = "wcphlex_frame_source_0",
    source_name_1 = "wcphlex_frame_source_1",
    source_name_2 = "wcphlex_frame_source_2",
    source_name_3 = "wcphlex_frame_source_3",
    sink_name_0   = "wcphlex_frame_sink",
    app_name      = "wcphlex_pgrapher",
)

local src_names = [source_name_0, source_name_1, source_name_2, source_name_3];

local srcs = [
    {
        type: "GenericFrameBoundarySource",
        name: src_names[n],
        data: {},
    }
    for n in std.range(0, 3)
];

local fanin = {
    type: "FrameFanin",
    name: "fanin",
    data: {
        multiplicity: 4,
        tags: ["apa0", "apa1", "apa2", "apa3"],
    },
};

local snk = {
    type: "GenericFrameBoundarySink",
    name: sink_name_0,
    data: {},
};

// Edges: each boundary source → FrameFanin input port N; FrameFanin → sink.
local edges = [
    {
        tail: { node: wc.tn(srcs[n]), port: 0 },
        head: { node: wc.tn(fanin),   port: n },
    }
    for n in std.range(0, 3)
] + [
    {
        tail: { node: wc.tn(fanin), port: 0 },
        head: { node: wc.tn(snk),   port: 0 },
    },
];

srcs + [fanin, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: { edges: edges },
}]
