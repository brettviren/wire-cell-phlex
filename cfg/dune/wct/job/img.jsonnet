// cfg/dune/wct/job/img.jsonnet
//
// WCT sub-graph: FrameBoundarySource -> [slicer] -> SliceFanout(2)
//                -> GridTiling(face0) + GridTiling(face1) -> BlobSetSync(2)
//                -> BlobSetBoundarySink
//
// The 3D-imaging (tiling) island for a single anode of any DUNE detector
// described by a DetectorDescription object (see wct/dets/).  It turns one input
// IFrame into a STREAM of IBlobSet (one per time slice); the Phlex-side
// CollectExecutor (wcph_frame_to_blobsets) collects the whole stream into a
// single wc.blobs product.
//
// Two slicers are supported, selected by the `slicer` TLA:
//
//   "sum"  (default) -- SumSlices: sums per-channel charge into slices,
//           skipping bins with q==0, i.e. slice on ANY positive charge.  This is
//           the natural "charge>0" slicing for the ideal DepoFluxSplat "true"
//           frame (untagged, noise-free): no gauss/wiener/error/summary tags are
//           needed.
//
//   "mask" -- MaskSlices: the signal-processing slicer, thresholding at
//           nthreshold * (per-channel wiener RMS).  Requires the SP frame's
//           gauss + wiener trace tags, the wiener trace_summary, and a
//           gauss_error frame.  OmnibusSigProc supplies gauss/wiener/summary but
//           NOT the error, so a ChargeErrorFrameEstimator pre-stage synthesizes
//           it (using the microboone charge-error map, as the reference PDHD
//           img.jsonnet does).  Use this for the OmnibusSigProc frame.
//
// Both slicers stream ISlice; the tiling (SliceFanout -> per-face GridTiling ->
// BlobSetSync) is identical for either.  3-view only (active_planes=[0,1,2]); the
// dead-channel multi-2view variants are deferred.
//
// TLA parameters (phlex passes all TLAs as strings):
//   sources  -- array of WCT inode { type, name } for the boundary source(s)
//   sinks    -- array of WCT inode { type, name } for the boundary sink(s)
//   app_name       (string): instance name for Pgrapher
//   detector       (string): canonical detector name, e.g. "pdhd" or "pdvd"
//   anode_index    (string): anode index into det.anodes[], e.g. "0"
//   service_prefix (string): prefix for all WCT service component names
//   slicer         (string): "sum" (charge>0, default) or "mask" (SP nthreshold)
//   tick_span      (string): time-slice thickness in ticks (default "4")
//   sum_tag        (string): "sum" slicer: trace tag to sum ("" = all traces)
//   nthreshold     (string): "mask" slicer: per-plane slice threshold in units of
//                            channel RMS, applied to all three planes (default "3.6")
//   charge_tag / wiener_tag / error_tag (string): "mask" trace tags, used as
//                            given; defaults gauss/wiener/gauss_error (OSP defaults)
//   charge_error_file (string): "mask" ChargeErrorFrameEstimator WaveformMap file
//
// Required WCT plugins: WireCellPgraph, WireCellImg (MaskSlices/SumSlices/
// SliceFanout/GridTiling/BlobSetSync), WireCellSigProc (ChargeErrorFrameEstimator,
// only for slicer="mask"), WireCellAux, WireCellGen.

local wc   = import "wirecell.jsonnet";
local dets = import "dune/wct/dets.jsonnet";

function(
    sources  = [],
    sinks    = [],
    app_name       = "wcphlex_pgrapher",
    detector       = "pdhd",
    anode_index    = "0",
    service_prefix = "",
    slicer         = "sum",
    tick_span      = "4",
    sum_tag        = "",
    nthreshold     = "3.6",
    charge_tag     = "gauss",
    wiener_tag     = "wiener",
    error_tag      = "gauss_error",
    charge_error_file = "microboone-charge-error.json.bz2",
)

local det = dets[detector]({detname: detector});
local ai  = std.parseInt(anode_index);
local a   = det.anodes[ai];

local span   = std.parseInt(tick_span);
local nthr   = std.parseJson(nthreshold);   // number
local use_mask = slicer == "mask";

// Trace tags used by the "mask" slicer, taken AS GIVEN (no ident suffix): the
// island is single-anode, and OmnibusSigProc tags its output traces with the
// bare "gauss"/"wiener" by default (sim-sigproc.jsonnet uses those defaults).
// A caller whose frame uses ident-suffixed tags (e.g. "gauss0") passes the full
// tag via the charge_tag/wiener_tag/error_tag TLAs.
local ct = charge_tag;
local wt = wiener_tag;
local et = error_tag;

// ---------------------------------------------------------------------------
// Service components
// ---------------------------------------------------------------------------

local wires = {
    type: "WireSchemaFile",
    name: service_prefix + "wires_" + a.name,
    data: { filename: det.wires.filename },
};

local anode = {
    type: "AnodePlane",
    name: service_prefix + a.name,
    data: {
        ident:       a.ident,
        nimpacts:    det.sim.nimpacts,
        wire_schema: wc.tn(wires),
        faces:       a.faces,
    },
};

// ---------------------------------------------------------------------------
// Optional pre-stage: synthesize the gauss_error frame for the "mask" slicer.
// (OmnibusSigProc emits gauss/wiener + the wiener summary, but no error frame.)
// ---------------------------------------------------------------------------

local waveform_map = {
    type: "WaveformMap",
    name: service_prefix + "wfm_" + a.name,
    data: { filename: charge_error_file },
};

local charge_err = {
    type: "ChargeErrorFrameEstimator",
    name: service_prefix + "cefe_" + a.name,
    data: {
        intag:         ct,
        outtag:        et,
        anode:         wc.tn(anode),
        rebin:         4,
        fudge_factors: [2.31, 2.31, 1.1],
        time_limits:   [12, 800],
        errors:        wc.tn(waveform_map),
    },
};

// ---------------------------------------------------------------------------
// Slicer
// ---------------------------------------------------------------------------

local sum_slicer = {
    type: "SumSlices",
    name: service_prefix + "sumslices_" + a.name,
    data: {
        tick_span: span,
        tag:       sum_tag,      // "" = all traces
        anode:     wc.tn(anode),
    },
};

local mask_slicer = {
    type: "MaskSlices",
    name: service_prefix + "maskslices_" + a.name,
    data: {
        tick_span:     span,
        wiener_tag:    wt,
        summary_tag:   wt,       // OSP puts per-channel thresholds in the wiener summary
        charge_tag:    ct,
        error_tag:     et,
        anode:         wc.tn(anode),
        // Both 0 => MaskSlices auto-derives the window from the input frame.
        min_tbin:      0,
        max_tbin:      0,
        active_planes: [0, 1, 2],
        nthreshold:    [nthr, nthr, nthr],
    },
};

local slicer_comp = if use_mask then mask_slicer else sum_slicer;

// ---------------------------------------------------------------------------
// Tiling: fan the per-slice stream to the two anode faces, tile each, re-sync.
// ---------------------------------------------------------------------------

local slice_fanout = {
    type: "SliceFanout",
    name: service_prefix + "slicefanout_" + a.name,
    data: { multiplicity: 2 },
};

local tilings = [
    {
        type: "GridTiling",
        name: service_prefix + "tiling_" + a.name + "_face" + face,
        data: {
            anode: wc.tn(anode),
            face:  face,
            nudge: 1e-2,
        },
    }
    for face in [0, 1]
];

local blobsync = {
    type: "BlobSetSync",
    name: service_prefix + "blobsetsync_" + a.name,
    data: { multiplicity: 2 },
};

// ---------------------------------------------------------------------------
// Boundary nodes
// ---------------------------------------------------------------------------

local src = sources[0] { data: {} };
local snk = sinks[0] { data: {} };

// ---------------------------------------------------------------------------
// Full component list + Pgrapher
// ---------------------------------------------------------------------------

// Edge from the boundary source into the slicer, optionally through the
// charge-error pre-stage (mask slicer only).
local front_edges = if use_mask then [
    { tail: { node: wc.tn(src),         port: 0 },
      head: { node: wc.tn(charge_err),  port: 0 } },
    { tail: { node: wc.tn(charge_err),  port: 0 },
      head: { node: wc.tn(slicer_comp), port: 0 } },
] else [
    { tail: { node: wc.tn(src),         port: 0 },
      head: { node: wc.tn(slicer_comp), port: 0 } },
];

[wires, anode] +
(if use_mask then [waveform_map, charge_err] else []) +
[slicer_comp, slice_fanout] + tilings + [blobsync, src, snk] +
[
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: front_edges + [
            { tail: { node: wc.tn(slicer_comp),  port: 0 },
              head: { node: wc.tn(slice_fanout), port: 0 } },
            { tail: { node: wc.tn(slice_fanout), port: 0 },
              head: { node: wc.tn(tilings[0]),   port: 0 } },
            { tail: { node: wc.tn(slice_fanout), port: 1 },
              head: { node: wc.tn(tilings[1]),   port: 0 } },
            { tail: { node: wc.tn(tilings[0]),   port: 0 },
              head: { node: wc.tn(blobsync),     port: 0 } },
            { tail: { node: wc.tn(tilings[1]),   port: 0 },
              head: { node: wc.tn(blobsync),     port: 1 } },
            { tail: { node: wc.tn(blobsync),     port: 0 },
              head: { node: wc.tn(snk),          port: 0 } },
        ],
    },
}]
