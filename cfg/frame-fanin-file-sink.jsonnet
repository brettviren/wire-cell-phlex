// cfg/frame-fanin-file-sink.jsonnet
//
// WCT sub-graph: 4×FrameBoundarySource → FrameFanin → FrameFileSink.
//
// Used by the FrameFaninSinkFile executor.  Each PHLEX event delivers four
// Frames (one per APA) to the four boundary sources.  FrameFanin merges them
// into a single monolithic Frame (concatenating traces, merging channel masks).
// FrameFileSink writes the merged Frame to disk.
//
// TLA parameters:
//   source_name_0..3 (string): instance names for the 4 FrameBoundarySource nodes
//   app_name         (string): instance name for the Pgrapher
//   outname          (string): output .npz file path (passed via wct_tla)
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellSio

local wc = import "wirecell.jsonnet";

function(
    source_name_0 = "wcphlex_frame_source_0",
    source_name_1 = "wcphlex_frame_source_1",
    source_name_2 = "wcphlex_frame_source_2",
    source_name_3 = "wcphlex_frame_source_3",
    app_name      = "wcphlex_pgrapher",
    outname       = "fanin-frames.npz",
)

local src_names = [source_name_0, source_name_1, source_name_2, source_name_3];

local srcs = [
    {
        type: "FrameBoundarySource",
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

local filesink = {
    type: "FrameFileSink",
    name: "filesink",
    data: {
        outname:  outname,
        digitize: false,
    },
};

// Edges: each boundary source → FrameFanin input port N
//        FrameFanin output port 0 → FrameFileSink
local edges = [
    {
        tail: { node: wc.tn(srcs[n]), port: 0 },
        head: { node: wc.tn(fanin),   port: n },
    }
    for n in std.range(0, 3)
] + [
    {
        tail: { node: wc.tn(fanin),    port: 0 },
        head: { node: wc.tn(filesink), port: 0 },
    },
];

srcs + [fanin, filesink,
{
    type: "Pgrapher",
    name: app_name,
    data: { edges: edges },
}]
