// cfg/pdhd-file-drifter.jsonnet
//
// WCT sub-graph: DepoFileSource → DepoSetDrifter → DepoSetBoundarySink
//
// Reads a depo file and drifts all depos to the response planes of all 4 PDHD
// APAs (2 drift columns × 2 faces each = 4 drift regions).  Used by the
// DepoSetSourceFile executor, which runs the graph once and queues all output
// DepoSets for per-event drain.
//
// The Drifter xregions cover both drift columns (APAs 0/2 on the negative-x
// side and APAs 1/3 on the positive-x side).  Each downstream per-APA
// DepoTransform uses its own AnodePlane to select only depos within its volume.
//
// PDHD geometry constants from wire-cell-toolkit simparams.jsonnet.
//
// TLA parameters:
//   sink_name (string): instance name for DepoSetBoundarySink
//   app_name  (string): instance name for Pgrapher
//   inname    (string): input depo file path (via wct_tla from module config)
//
// Required WCT plugins: WireCellPgraph, WireCellSio, WireCellGen

local wc = import "wirecell.jsonnet";

function(
    sink_name = "wcphlex_deposet_sink",
    app_name  = "wcphlex_pgrapher",
    inname    = "depos.npz",
)

// ---------------------------------------------------------------------------
// PDHD geometry constants (from wire-cell-toolkit simparams.jsonnet)
// ---------------------------------------------------------------------------

local apa_cpa   = 3.5734  * wc.m;
local cpa_thick = 3.175   * wc.mm;
local apa_w2w   = 85.87   * wc.mm;
local plane_gap = 4.76    * wc.mm;
local apa_g2g   = apa_w2w + 6 * plane_gap;
local apa_plane = 0.5 * apa_g2g - plane_gap;      // 52.455 mm from APA centerline to anode wire plane
local res_dist  = 10 * wc.cm;
local res_plane = 0.5 * apa_w2w + res_dist;        // 142.935 mm from APA centerline to response plane
local cpa_plane = apa_cpa - 0.5 * cpa_thick;       // 3571.8125 mm from APA centerline to cathode

// Compute the two faces for APA n.
// n%2 == 0 → sign = -1 (left drift column, APAs 0 and 2)
// n%2 == 1 → sign = +1 (right drift column, APAs 1 and 3)
local apa_faces(n) =
    local sign = 2 * (n % 2) - 1;
    local cl = sign * apa_cpa;
    [
        { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
        { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane },
    ];

// All 4 drift regions (APAs 0 and 1 cover both drift columns).
// APAs 2 and 3 share the same x-positions as APAs 0 and 1 respectively;
// the wire schema distinguishes them by y-coordinate channel mapping.
local xregions = apa_faces(0) + apa_faces(1);

// ---------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------

local rng = {
    type: "Random",
    name: "rng",
    data: {},
};

// ---------------------------------------------------------------------------
// Drifter (whole-detector: covers all 4 xregions)
// ---------------------------------------------------------------------------

local drifter_comp = {
    type: "Drifter",
    name: "drifter",
    data: {
        rng:         wc.tn(rng),
        DL:          7.2 * wc.cm2 / wc.s,
        DT:          12.0 * wc.cm2 / wc.s,
        lifetime:    8 * wc.ms,
        drift_speed: 1.6 * wc.mm / wc.us,
        fluctuate:   false,
        xregions:    xregions,
    },
};

local setdrifter = {
    type: "DepoSetDrifter",
    name: "deposet_drifter",
    data: { drifter: wc.tn(drifter_comp) },
};

// ---------------------------------------------------------------------------
// Source, sink, graph
// ---------------------------------------------------------------------------

local file_source = {
    type: "DepoFileSource",
    name: "file_source",
    data: { inname: inname },
};

local snk = {
    type: "DepoSetBoundarySink",
    name: sink_name,
    data: {},
};

[rng, drifter_comp, setdrifter, file_source, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            {
                tail: { node: wc.tn(file_source),  port: 0 },
                head: { node: wc.tn(setdrifter),   port: 0 },
            },
            {
                tail: { node: wc.tn(setdrifter),   port: 0 },
                head: { node: wc.tn(snk),           port: 0 },
            },
        ],
    },
}]
