// cfg/deposet-drift-sim.jsonnet
//
// WCT sub-graph: GenericDepoSetBoundarySource → DepoSetDrifter → DepoTransform → GenericFrameBoundarySink
//
// Implements a full drift + electronics simulation pipeline using PDSP APA 0
// geometry parameters.  Used by the DepoSetToFrame executor.
//
// Data files required (via WIRECELL_PATH):
//   protodune-wires-larsoft-v4.json.bz2   — wire geometry (WireCellAux)
//   dune-garfield-1d565.json.bz2          — field response (WireCellSigProc)
//
// TLA parameters (injected by FunctionExecutor<IDepoSet,IFrame> — indexed per
// port; single-port shapes use index 0):
//   source_name_0 (string): instance name for GenericDepoSetBoundarySource
//   sink_name_0   (string): instance name for GenericFrameBoundarySink
//   app_name      (string): instance name for Pgrapher
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellSigProc, WireCellAux

local wc = import "wirecell.jsonnet";

function(
    source_name_0  = "wcphlex_deposet_source",
    sink_name_0    = "wcphlex_frame_sink",
    app_name       = "wcphlex_pgrapher",
    service_prefix = "",   // prefix for all service component names;
                           // "" (default) = bare names, shared with any other island
                           // that uses the same default; non-empty = independent copies
)

local tick           = 0.5 * wc.us;
local nticks_daq     = 10000;   // DAQ readout ticks
local response_nticks = 125;    // field response headroom: 62.5µs / 0.5µs
local nticks_ductor  = nticks_daq + response_nticks;  // 10125 total
local readout_time   = nticks_ductor * tick;   // 5.0625 ms
local start_time     = -62.5 * wc.us;          // -response_time = start of DepoTransform window
local drift_speed    = 1.6 * wc.mm / wc.us;

// ---------------------------------------------------------------------------
// Services (shared, no data file needed)
// ---------------------------------------------------------------------------

local dft = {
    type: "FftwDFT",
    name: service_prefix + "dft",
    data: {},
};

local rng = {
    type: "Random",
    name: service_prefix + "rng",
    data: { seed: 1 },   // fixed seed for reproducible noise; change to 0 for time-based
};

// ---------------------------------------------------------------------------
// Geometry and field response data files
// ---------------------------------------------------------------------------

local wires = {
    type: "WireSchemaFile",
    name: service_prefix + "wires",
    data: { filename: "protodune-wires-larsoft-v4.json.bz2" },
};

local fr = {
    type: "FieldResponse",
    name: service_prefix + "fr",
    data: { filename: "dune-garfield-1d565.json.bz2" },
};

// ---------------------------------------------------------------------------
// Electronics response
// ---------------------------------------------------------------------------

local elec = {
    type: "ColdElecResponse",
    name: service_prefix + "elec",
    data: {
        tick:     tick,
        nticks:   nticks_ductor,
        shaping:  2.2 * wc.us,         // PDSP shaping time; must match frame-sigproc.jsonnet
        gain:     14.0 * wc.mV / wc.fC,
        postgain: 1.0,
    },
};

// ---------------------------------------------------------------------------
// PDSP APA 0 face geometry (from wire-cell-toolkit simparams.jsonnet)
//
//   Face 0 (front):  anode=-3578.36  response=-3487.8875  cathode=-1.5875
//   Face 1 (back):   anode=-3683.14  response=-3773.6125  cathode=-7259.9125
//
// Units: mm (WCT default system units for spatial coordinates).
// These values serve dual purpose: AnodePlane faces AND Drifter xregions.
// ---------------------------------------------------------------------------

local faces = [
    { anode: -3578.36 * wc.mm, response: -3487.8875 * wc.mm, cathode: -1.5875 * wc.mm },
    { anode: -3683.14 * wc.mm, response: -3773.6125 * wc.mm, cathode: -7259.9125 * wc.mm },
];

// ---------------------------------------------------------------------------
// Anode plane
// ---------------------------------------------------------------------------

local anode = {
    type: "AnodePlane",
    name: service_prefix + "apa0",
    data: {
        ident:       0,
        nimpacts:    10,
        wire_schema: wc.tn(wires),
        faces:       faces,
    },
};

// ---------------------------------------------------------------------------
// Plane impact responses (one per plane: U=0, V=1, W=2)
// ---------------------------------------------------------------------------

local pir(plane) = {
    type: "PlaneImpactResponse",
    name: service_prefix + "pir%d" % plane,
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

// ---------------------------------------------------------------------------
// Drifter  (xregions must match AnodePlane faces)
// ---------------------------------------------------------------------------

local drifter_comp = {
    type: "Drifter",
    name: service_prefix + "drifter",
    data: {
        rng:         wc.tn(rng),
        DL:          7.2 * wc.cm2 / wc.s,
        DT:          12.0 * wc.cm2 / wc.s,
        lifetime:    8 * wc.ms,
        drift_speed: drift_speed,
        fluctuate:   false,
        xregions:    faces,
    },
};

local setdrifter = {
    type: "DepoSetDrifter",
    name: service_prefix + "deposet_drifter",
    data: { drifter: wc.tn(drifter_comp) },
};

// ---------------------------------------------------------------------------
// DepoTransform (IDepoSet → IFrame)
// ---------------------------------------------------------------------------

local transform = {
    type: "DepoTransform",
    name: service_prefix + "transform",
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

// ---------------------------------------------------------------------------
// Reframer: crops the 125-tick field-response headroom from DepoTransform
// output and forces tbin=0 on all output traces (one per anode channel).
// Required by OmnibusSigProc which assumes tbin==0 on every input trace.
// ---------------------------------------------------------------------------

local reframer = {
    type: "Reframer",
    name: service_prefix + "reframer",
    data: {
        anode:   wc.tn(anode),
        tags:    [],
        fill:    0.0,
        tbin:    response_nticks,   // skip first 125 response-headroom ticks
        toffset: 0,
        nticks:  nticks_daq,        // output exactly nticks_daq ticks, tbin=0
    },
};

// ---------------------------------------------------------------------------
// Noise model + AddNoise (empirical PDSP noise spectra)
// ---------------------------------------------------------------------------

local noise_model = {
    type: "EmpiricalNoiseModel",
    name: service_prefix + "noise_model",
    data: {
        anode:             wc.tn(anode),
        dft:               wc.tn(dft),
        chanstat:          "",
        spectra_file:      "protodune-noise-spectra-v1.json.bz2",
        nsamples:          nticks_daq,
        period:            tick,
        wire_length_scale: 1.0 * wc.cm,
    },
};

local addnoise = {
    type: "AddNoise",
    name: service_prefix + "addnoise",
    data: {
        rng:                    wc.tn(rng),
        dft:                    wc.tn(dft),
        model:                  wc.tn(noise_model),
        nsamples:               nticks_daq,
        replacement_percentage: 0.02,
    },
};

// ---------------------------------------------------------------------------
// Digitizer: converts floating-point voltage traces to integer ADC counts.
// PDSP: 12-bit ADC, fullscale 0.2–1.6 V, per-plane baselines.
// ---------------------------------------------------------------------------

local digitizer = {
    type: "Digitizer",
    name: service_prefix + "digitizer",
    data: {
        anode:     wc.tn(anode),
        resolution: 12,
        gain:       1.0,
        fullscale:  [0.2 * wc.volt, 1.6 * wc.volt],
        baselines:  [1003.4 * wc.mV, 1003.4 * wc.mV, 507.7 * wc.mV],
    },
};

// ---------------------------------------------------------------------------
// Boundary nodes (names come from TLAs injected by DepoSetToFrame executor)
// ---------------------------------------------------------------------------

local src = {
    type: "GenericDepoSetBoundarySource",
    name: source_name_0,
    data: {},
};

local snk = {
    type: "GenericFrameBoundarySink",
    name: sink_name_0,
    data: {},
};

// ---------------------------------------------------------------------------
// Full component list + Pgrapher
// ---------------------------------------------------------------------------

[dft, rng, wires, fr, elec, anode] + pirs +
[drifter_comp, setdrifter, transform, reframer, noise_model, addnoise, digitizer, src, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            {
                tail: { node: wc.tn(src),        port: 0 },
                head: { node: wc.tn(setdrifter),  port: 0 },
            },
            {
                tail: { node: wc.tn(setdrifter),  port: 0 },
                head: { node: wc.tn(transform),   port: 0 },
            },
            {
                tail: { node: wc.tn(transform),   port: 0 },
                head: { node: wc.tn(reframer),    port: 0 },
            },
            {
                tail: { node: wc.tn(reframer),    port: 0 },
                head: { node: wc.tn(addnoise),    port: 0 },
            },
            {
                tail: { node: wc.tn(addnoise),    port: 0 },
                head: { node: wc.tn(digitizer),   port: 0 },
            },
            {
                tail: { node: wc.tn(digitizer),   port: 0 },
                head: { node: wc.tn(snk),          port: 0 },
            },
        ],
    },
}]
