// cfg/dune/wct/dets/pdhd/sp-filters.jsonnet
//
// ProtoDUNE-HD (PDHD) tuned LfFilter and HfFilter objects required by OmnibusSigProc.
//
// WARNING: OmnibusSigProc hard-codes the instance names of all filter components
// in its C++ implementation.  The names below MUST NOT be changed; they are looked
// up by WCT's named factory at configure() time.
//
// PDHD has 16 filter instances (13 base + 3 APA1-variant Wiener filters).
// The APA1-variant names (Wiener_tight_*_APA1) are used for APA0 which uses a
// higher-quality field response fit; the standard names are used for APAs 1-3.
//
// Transcribed from dunereco/DUNEWireCell/pdhd/sp-filters.jsonnet.
//
// Required WCT plugins: WireCellSigProc (provides LfFilter and HfFilter)

local wc = import "wirecell.jsonnet";

local lf(name, data={}) = {
    type: "LfFilter",
    name: name,
    data: {
        max_freq: 1 * wc.megahertz,
        tau: 0.0 * wc.megahertz,
    } + data,
};

local hf(name, data={}) = {
    type: "HfFilter",
    name: name,
    data: {
        max_freq: 1 * wc.megahertz,
        sigma: 0.0 * wc.megahertz,
        power: 2,
        flag: true,
    } + data,
};

local wf(name, data={}) = {
    type: "HfFilter",
    name: name,
    data: {
        max_freq: 1,
        power: 2,
        flag: false,
        sigma: 0.0,
    } + data,
};

[
    // Low-frequency (LF) filters
    lf("ROI_loose_lf",   { tau: 0.002 * wc.megahertz }),
    lf("ROI_tight_lf",   { tau: 0.016 * wc.megahertz }),
    lf("ROI_tighter_lf", { tau: 0.08  * wc.megahertz }),

    // High-frequency Gaussian filters
    hf("Gaus_tight"),
    hf("Gaus_wide",  { sigma: 0.12 * wc.megahertz }),

    // Wiener filters — standard set (APAs 1-3)
    hf("Wiener_tight_U", { sigma: 0.221933 * wc.megahertz, power: 6.55413 }),
    hf("Wiener_tight_V", { sigma: 0.222723 * wc.megahertz, power: 8.75998 }),
    hf("Wiener_tight_W", { sigma: 0.225567 * wc.megahertz, power: 3.47846 }),

    // Wiener filters — APA1-variant (used for APA0 which has the 6-path MCMC FR)
    hf("Wiener_tight_U_APA1", { sigma: 0.203451 * wc.megahertz, power: 5.78093 }),
    hf("Wiener_tight_V_APA1", { sigma: 0.160191 * wc.megahertz, power: 3.54835 }),
    hf("Wiener_tight_W_APA1", { sigma: 0.125448 * wc.megahertz, power: 5.27080 }),

    // Wide Wiener filters
    hf("Wiener_wide_U", { sigma: 0.186765 * wc.megahertz, power: 5.05429 }),
    hf("Wiener_wide_V", { sigma: 0.1936   * wc.megahertz, power: 5.77422 }),
    hf("Wiener_wide_W", { sigma: 0.175722 * wc.megahertz, power: 4.37928 }),

    // Wire (deconvolution) filters
    wf("Wire_ind", { sigma: 1.0 / wc.sqrtpi * 0.75 }),
    wf("Wire_col", { sigma: 1.0 / wc.sqrtpi * 10.0 }),
]
