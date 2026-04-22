// cfg/deposet-sim-sigproc.jsonnet
//
// WCT sub-graph: DepoSetBoundarySource → DepoSetDrifter → DepoTransform
//                → OmnibusSigProc → FrameBoundarySink.
//
// Combined drift + electronics simulation + signal processing variant
// (Variant B of third-real-job).  Used by the DepoSetToFrame executor.
//
// Extends deposet-drift-sim.jsonnet by appending OmnibusSigProc after
// DepoTransform.  All shared geometry / response objects (AnodePlane,
// FftwDFT, FieldResponse, ColdElecResponse) are reused by both
// DepoTransform and OmnibusSigProc.
//
// Data files required (via WIRECELL_PATH):
//   protodune-wires-larsoft-v4.json.bz2  — wire geometry
//   dune-garfield-1d565.json.bz2         — field response
//
// TLA parameters:
//   source_name (string): instance name for DepoSetBoundarySource
//   sink_name   (string): instance name for FrameBoundarySink
//   app_name    (string): instance name for Pgrapher
//
// Required WCT plugins: WireCellPgraph, WireCellGen, WireCellSigProc, WireCellAux

local wc = import "wirecell.jsonnet";
local spfilt = import "sp-filters.jsonnet";

function(
    source_name = "wcphlex_deposet_source",
    sink_name   = "wcphlex_frame_sink",
    app_name    = "wcphlex_pgrapher",
)

local tick            = 0.5 * wc.us;
local nticks_daq      = 10000;   // DAQ readout ticks
local response_nticks = 125;     // field response headroom: 62.5µs / 0.5µs
local nticks_ductor   = nticks_daq + response_nticks;  // 10125 total
local readout_time    = nticks_ductor * tick;
local start_time      = -62.5 * wc.us;   // -response_time = start of DepoTransform window
local drift_speed     = 1.6 * wc.mm / wc.us;

// ---------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------

local dft = {
    type: "FftwDFT",
    name: "dft",
    data: {},
};

local rng = {
    type: "Random",
    name: "rng",
    data: {},
};

// ---------------------------------------------------------------------------
// Geometry and field response data files
// ---------------------------------------------------------------------------

local wires = {
    type: "WireSchemaFile",
    name: "wires",
    data: { filename: "protodune-wires-larsoft-v4.json.bz2" },
};

local fr = {
    type: "FieldResponse",
    name: "fr",
    data: { filename: "dune-garfield-1d565.json.bz2" },
};

// ---------------------------------------------------------------------------
// Electronics response
//
// For DepoTransform:  used as IWaveform[] in short_responses.
// For OmnibusSigProc: used as IWaveform via elecresponse key.
// Both resolve to the same ColdElecResponse instance by type-name lookup.
// ---------------------------------------------------------------------------

local elec = {
    type: "ColdElecResponse",
    name: "elec",
    data: {
        tick:     tick,
        nticks:   nticks_ductor,
        shaping:  2.2 * wc.us,         // PDSP shaping time
        gain:     14.0 * wc.mV / wc.fC,
        postgain: 1.0,
    },
};

// ---------------------------------------------------------------------------
// PDSP APA 0 face geometry
// ---------------------------------------------------------------------------

local faces = [
    { anode: -3578.36 * wc.mm, response: -3487.8875 * wc.mm, cathode: -1.5875 * wc.mm },
    { anode: -3683.14 * wc.mm, response: -3773.6125 * wc.mm, cathode: -7259.9125 * wc.mm },
];

local anode = {
    type: "AnodePlane",
    name: "apa0",
    data: {
        ident:       0,
        nimpacts:    10,
        wire_schema: wc.tn(wires),
        faces:       faces,
    },
};

// ---------------------------------------------------------------------------
// PlaneImpactResponse (3×, for DepoTransform)
// ---------------------------------------------------------------------------

local pir(plane) = {
    type: "PlaneImpactResponse",
    name: "pir%d" % plane,
    data: {
        plane:                 plane,
        dft:                   wc.tn(dft),
        field_response:        wc.tn(fr),
        nticks:                nticks_ductor,
        tick:                  tick,
        short_responses:       [wc.tn(elec)],
        overall_short_padding: 0.1 * wc.ms,
        long_responses:        [],
        long_padding:          1.5 * wc.ms,
    },
};
local pirs = [pir(n) for n in [0, 1, 2]];

// ---------------------------------------------------------------------------
// Drifter
// ---------------------------------------------------------------------------

local drifter_comp = {
    type: "Drifter",
    name: "drifter",
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
    name: "deposet_drifter",
    data: { drifter: wc.tn(drifter_comp) },
};

// ---------------------------------------------------------------------------
// DepoTransform (IDepoSet → IFrame)
// ---------------------------------------------------------------------------

local transform = {
    type: "DepoTransform",
    name: "transform",
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
// Reframer: crops the 125-tick field-response headroom and forces tbin=0.
// OmnibusSigProc requires tbin==0 on every input trace.
// ---------------------------------------------------------------------------

local reframer = {
    type: "Reframer",
    name: "reframer",
    data: {
        anode:   wc.tn(anode),
        tags:    [],
        fill:    0.0,
        tbin:    response_nticks,
        toffset: 0,
        nticks:  nticks_daq,
    },
};

// ---------------------------------------------------------------------------
// Noise model + AddNoise (empirical PDSP noise spectra)
// ---------------------------------------------------------------------------

local noise_model = {
    type: "EmpiricalNoiseModel",
    name: "noise_model",
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
    name: "addnoise",
    data: {
        rng:                    wc.tn(rng),
        dft:                    wc.tn(dft),
        model:                  wc.tn(noise_model),
        nsamples:               nticks_daq,
        replacement_percentage: 0.02,
    },
};

// ---------------------------------------------------------------------------
// Digitizer: floating-point voltage traces → integer ADC counts.
// PDSP: 12-bit ADC, fullscale 0.2–1.6 V.
// ---------------------------------------------------------------------------

local digitizer = {
    type: "Digitizer",
    name: "digitizer",
    data: {
        anode:      wc.tn(anode),
        resolution: 12,
        gain:       1.0,
        fullscale:  [0.2 * wc.volt, 1.6 * wc.volt],
        baselines:  [1003.4 * wc.mV, 1003.4 * wc.mV, 507.7 * wc.mV],
    },
};

// ---------------------------------------------------------------------------
// OmnibusSigProc (IFrame → IFrame)
// ---------------------------------------------------------------------------

local sigproc = {
    type: "OmnibusSigProc",
    name: "sigproc",
    data: {
        anode:          wc.tn(anode),
        dft:            wc.tn(dft),
        field_response: wc.tn(fr),
        elecresponse:   wc.tn(elec),
        per_chan_resp:   "",
        ftoffset:       0.0,
        ctoffset:       1.0 * wc.us,
        postgain:       1.0,
        ADC_mV:         4095.0 / (1.4 * wc.volt),
        fft_flag:       0,
        troi_col_th_factor: 5.0,
        troi_ind_th_factor: 3.0,
        lroi_rebin:         6,
        lroi_th_factor:     3.5,
        lroi_th_factor1:    0.7,
        lroi_jump_one_bin:  1,
        r_th_factor:        3.0,
        r_fake_signal_low_th:    375,
        r_fake_signal_high_th:   750,
        r_fake_signal_low_th_ind_factor:  1.0,
        r_fake_signal_high_th_ind_factor: 1.0,
        r_th_peak:      3.0,
        r_sep_peak:     6.0,
        r_low_peak_sep_threshold_pre: 1200,
        use_roi_debug_mode:     false,
        use_multi_plane_protection: false,
        isWrapped:      false,
        sparse:         true,
    },
};

// ---------------------------------------------------------------------------
// Boundary nodes
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Full component list + Pgrapher
// ---------------------------------------------------------------------------

[dft, rng, wires, fr, elec, anode] + pirs +
[drifter_comp, setdrifter, transform, reframer, noise_model, addnoise, digitizer, sigproc]
+ spfilt
+ [src, snk,
{
    type: "Pgrapher",
    name: app_name,
    data: {
        edges: [
            {
                tail: { node: wc.tn(src),         port: 0 },
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
                head: { node: wc.tn(sigproc),     port: 0 },
            },
            {
                tail: { node: wc.tn(sigproc),     port: 0 },
                head: { node: wc.tn(snk),          port: 0 },
            },
        ],
    },
}]
