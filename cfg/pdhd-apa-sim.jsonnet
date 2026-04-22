// cfg/pdhd-apa-sim.jsonnet
//
// WCT sub-graph: DepoSetBoundarySource → DepoTransform → FrameBoundarySink
//
// Like pdhd-apa-sim-sigproc.jsonnet but without OmnibusSigProc.
// Used for debugging fan-out/fan-in topology when sigproc is not needed.

local wc = import "wirecell.jsonnet";

function(
    source_name = "wcphlex_deposet_source",
    sink_name   = "wcphlex_frame_sink",
    app_name    = "wcphlex_pgrapher",
    apa_ident   = "0",
)

local tick          = 0.5 * wc.us;
local nticks_ductor = 6000;
local readout_time  = nticks_ductor * tick;
local start_time    = -62.5 * wc.us;
local drift_speed   = 1.6 * wc.mm / wc.us;

local apa_cpa   = 3.5734  * wc.m;
local cpa_thick = 3.175   * wc.mm;
local apa_w2w   = 85.87   * wc.mm;
local plane_gap = 4.76    * wc.mm;
local apa_g2g   = apa_w2w + 6 * plane_gap;
local apa_plane = 0.5 * apa_g2g - plane_gap;
local res_dist  = 10 * wc.cm;
local res_plane = 0.5 * apa_w2w + res_dist;
local cpa_plane = apa_cpa - 0.5 * cpa_thick;

local all_faces = {
    "0": local cl = -apa_cpa;
         [ { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
           { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane } ],
    "1": local cl = +apa_cpa;
         [ { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
           { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane } ],
    "2": local cl = -apa_cpa;
         [ { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
           { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane } ],
    "3": local cl = +apa_cpa;
         [ { anode: cl + apa_plane, response: cl + res_plane, cathode: cl + cpa_plane },
           { anode: cl - apa_plane, response: cl - res_plane, cathode: cl - cpa_plane } ],
};

local all_idents = { "0": 0, "1": 1, "2": 2, "3": 3 };

local faces    = all_faces[apa_ident];
local apa_num  = all_idents[apa_ident];

local dft = {
    type: "FftwDFT",
    name: "dft_apa" + apa_ident,
    data: {},
};

local wires = {
    type: "WireSchemaFile",
    name: "wires_apa" + apa_ident,
    data: { filename: "protodunehd-wires-larsoft-v1.json.bz2" },
};

local fr = {
    type: "FieldResponse",
    name: "fr_apa" + apa_ident,
    data: { filename: "dune-garfield-1d565.json.bz2" },
};

local elec = {
    type: "ColdElecResponse",
    name: "elec_apa" + apa_ident,
    data: {
        tick:     tick,
        nticks:   nticks_ductor,
        shaping:  2.2 * wc.us,
        gain:     14.0 * wc.mV / wc.fC,
        postgain: 1.0,
    },
};

local anode = {
    type: "AnodePlane",
    name: "apa" + apa_ident,
    data: {
        ident:       apa_num,
        nimpacts:    10,
        wire_schema: wc.tn(wires),
        faces:       faces,
    },
};

local pir(plane) = {
    type: "PlaneImpactResponse",
    name: "pir%d_apa" % [plane] + apa_ident,
    data: {
        plane:                  plane,
        dft:                    wc.tn(dft),
        field_response:         wc.tn(fr),
        nticks:                 nticks_ductor,
        tick:                   tick,
        short_responses:        [wc.tn(elec)],
        overall_short_padding:  0.1 * wc.ms,
        long_responses:         [],
        long_padding:           1.5 * wc.ms,
    },
};
local pirs = [pir(n) for n in [0, 1, 2]];

local transform = {
    type: "DepoTransform",
    name: "transform_apa" + apa_ident,
    data: {
        anode:              wc.tn(anode),
        pirs:               [wc.tn(p) for p in pirs],
        dft:                wc.tn(dft),
        fluctuate:          false,
        drift_speed:        drift_speed,
        readout_time:       readout_time,
        start_time:         start_time,
        tick:               tick,
        nsigma:             3,
        first_frame_number: 0,
    },
};

local src = {
    type: "DepoSetBoundarySource",
    name: source_name,
    data: {},
};

local snk = {
    type: "FrameBoundarySink",
    name: sink_name,
    data: {},
};

[dft, wires, fr, elec, anode] + pirs + [transform, src, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            {
                tail: { node: wc.tn(src),       port: 0 },
                head: { node: wc.tn(transform),  port: 0 },
            },
            {
                tail: { node: wc.tn(transform),  port: 0 },
                head: { node: wc.tn(snk),         port: 0 },
            },
        ],
    },
}]
