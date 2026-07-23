// cfg/frame-fanin.jsonnet
//
// WCT sub-graph: N × Frame boundary source → FrameFanin → Frame boundary sink.
//
// The fan-in shape: each PHLEX event delivers N Frames (one per APA) to the N
// boundary sources; FrameFanin merges them into a single Frame (concatenating
// traces, merging channel masks) which is drained back to PHLEX through the
// boundary sink.  (Writing the merged Frame to a file is a separate sink node —
// see frame-file-sink.jsonnet.)
//
// The multiplicity is taken from the number of source inodes supplied, so the
// config adapts to whatever fan-in width the executor was built for.
//
// TLA parameters (injected by the ShapeExecutor base):
//   sources  — array of WCT inode objects { type, name }: the N boundary sources
//   sinks    — array of WCT inode objects { type, name }: one boundary sink
//   app_name — instance name for the Pgrapher
//
// Required WCT plugins: WireCellPgraph, WireCellGen

local wc = import "wirecell.jsonnet";

function(
    sources  = [],
    sinks    = [],
    app_name = "wcphlex_pgrapher",
)

local n = std.length(sources);
local srcs = [sources[i] { data: {} } for i in std.range(0, n - 1)];
local snk = sinks[0];

local fanin = {
    type: "FrameFanin",
    name: "fanin",
    data: {
        multiplicity: n,
        tags: ["apa%d" % i for i in std.range(0, n - 1)],
    },
};

// Edges: each boundary source → FrameFanin input port i; FrameFanin → sink.
local edges = [
    {
        tail: { node: wc.tn(srcs[i]), port: 0 },
        head: { node: wc.tn(fanin),   port: i },
    }
    for i in std.range(0, n - 1)
] + [
    {
        tail: { node: wc.tn(fanin),                 port: 0 },
        head: { node: snk.type + ":" + snk.name,    port: 0 },
    },
];

srcs + [fanin, snk { data: {} },
{
    type: "Pgrapher",
    name: app_name,
    data: { edges: edges },
}]
