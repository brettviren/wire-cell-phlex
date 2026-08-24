// cfg/dune/wct/dets/pdhd/sp-filters-narrow.jsonnet
//
// PDHD SP filters with NARROW Wiener_tight filters, for the bare-OmnibusSigProc
// path (opt-in via the detector's sp_wiener="narrow" param; xerosere ddm-3bu.9).
//
// Identical to sp-filters.jsonnet EXCEPT the Wiener_tight sigma/power values are
// replaced with the PDSP-tuned (narrower) set.  Rationale: PDHD's production
// Wiener_tight (sigma ~0.222) is tuned for PDHD's native np04hd field response
// and is meant to feed DNNROI, not the bare-OSP threshold ROI finder.  When the
// runtime pairs it with the generic dune-garfield-1d565 response and bare OSP,
// it UNDER-suppresses induction-deconvolution noise and the U/V planes flood
// with tens of thousands of small noise ROIs.  Swapping in the PDSP-tuned narrow
// Wiener (sigma ~0.148) restores induction noise suppression on that path
// (measured: event0 PDHD APA0 ideal line, SP ROIs 17416 -> 1082, matching PDSP).
//
// This is the OPT-IN variant; the DEFAULT sp-filters.jsonnet keeps the
// production-faithful (broad) values.  The narrow Wiener values are borrowed
// verbatim from pgrapher/experiment/pdsp/sp-filters.jsonnet ("Optimized May 2019").
//
// WARNING: OmnibusSigProc hard-codes these instance names; do not rename them.

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
    // Low-frequency (LF) filters -- unchanged from the default set.
    lf("ROI_loose_lf",   { tau: 0.002 * wc.megahertz }),
    lf("ROI_tight_lf",   { tau: 0.016 * wc.megahertz }),
    lf("ROI_tighter_lf", { tau: 0.08  * wc.megahertz }),

    // High-frequency Gaussian filters -- unchanged.
    hf("Gaus_tight"),
    hf("Gaus_wide",  { sigma: 0.12 * wc.megahertz }),

    // Wiener filters -- standard set (APAs 1-3), NARROWED to PDSP values.
    hf("Wiener_tight_U", { sigma: 0.148788  * wc.megahertz, power: 3.76194 }),
    hf("Wiener_tight_V", { sigma: 0.1596568 * wc.megahertz, power: 4.36125 }),
    hf("Wiener_tight_W", { sigma: 0.13623   * wc.megahertz, power: 3.35324 }),

    // Wiener filters -- APA1-variant (APA0), also NARROWED to PDSP values for the
    // bare-OSP path (used only when apa0_asbuilt=true).
    hf("Wiener_tight_U_APA1", { sigma: 0.148788  * wc.megahertz, power: 3.76194 }),
    hf("Wiener_tight_V_APA1", { sigma: 0.1596568 * wc.megahertz, power: 4.36125 }),
    hf("Wiener_tight_W_APA1", { sigma: 0.13623   * wc.megahertz, power: 3.35324 }),

    // Wide Wiener filters -- unchanged.
    hf("Wiener_wide_U", { sigma: 0.186765 * wc.megahertz, power: 5.05429 }),
    hf("Wiener_wide_V", { sigma: 0.1936   * wc.megahertz, power: 5.77422 }),
    hf("Wiener_wide_W", { sigma: 0.175722 * wc.megahertz, power: 4.37928 }),

    // Wire (deconvolution) filters -- unchanged.
    wf("Wire_ind", { sigma: 1.0 / wc.sqrtpi * 0.75 }),
    wf("Wire_col", { sigma: 1.0 / wc.sqrtpi * 10.0 }),
]
