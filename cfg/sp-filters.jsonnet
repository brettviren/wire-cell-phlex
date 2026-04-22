// cfg/sp-filters.jsonnet
//
// PDSP-tuned LfFilter and HfFilter objects required by OmnibusSigProc.
//
// WARNING: OmnibusSigProc hard-codes the instance names of all 13 filter
// components in its C++ implementation.  The names below MUST NOT be changed;
// they are looked up by WCT's named factory at configure() time.
//
// This file returns a Jsonnet array; include it in a config with:
//   local spfilt = import "sp-filters.jsonnet";
//   // Then add spfilt to your component list.
//
// Parameters adapted from:
//   wire-cell-toolkit/cfg/pgrapher/experiment/pdsp/sp-filters.jsonnet
//
// Required WCT plugins: WireCellSigProc (provides LfFilter and HfFilter)

local wc = import "wirecell.jsonnet";
local sqrtpi = std.sqrt(std.acos(-1));  // √π

// Helper constructors
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

// Wire (HfFilter with flag:false and dimensionless max_freq)
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
    // Low-frequency (LF) filters — 3 instances
    lf("ROI_tight_lf",   { tau: 0.014  * wc.megahertz }),
    lf("ROI_tighter_lf", { tau: 0.06   * wc.megahertz }),
    lf("ROI_loose_lf",   { tau: 0.002  * wc.megahertz }),

    // High-frequency (HF) Gaussian/Wiener filters — 8 instances
    hf("Gaus_tight"),   // default sigma=0 (placeholder; not the primary Gaussian)
    hf("Gaus_wide",  { sigma: 0.12     * wc.megahertz, power: 2 }),

    hf("Wiener_tight_U", { sigma: 0.148788  * wc.megahertz, power: 3.76194 }),
    hf("Wiener_tight_V", { sigma: 0.1596568 * wc.megahertz, power: 4.36125 }),
    hf("Wiener_tight_W", { sigma: 0.13623   * wc.megahertz, power: 3.35324 }),

    hf("Wiener_wide_U",  { sigma: 0.186765  * wc.megahertz, power: 5.05429 }),
    hf("Wiener_wide_V",  { sigma: 0.1936    * wc.megahertz, power: 5.77422 }),
    hf("Wiener_wide_W",  { sigma: 0.175722  * wc.megahertz, power: 4.37928 }),

    // Wire (deconvolution) filters — 2 instances (dimensionless sigma)
    wf("Wire_ind", { sigma: 1.0 / sqrtpi * 0.75  }),
    wf("Wire_col", { sigma: 1.0 / sqrtpi * 10.0 }),
]
