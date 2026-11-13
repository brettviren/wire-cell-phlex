/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// modules/gdml_geometry.cpp -> libwcph_gdml_geometry.so
//
// Two monolithic transforms from a GDML document (gdml::DocumentPtr, e.g.
// made by phlex-gdml's pgd_provider at the job layer) to what WCT needs:
//
//   wcph_gdml_wire_schema : gdml -> wcphlex::WireSchema  (suffix "wire_schema")
//   wcph_gdml_drift_planes : gdml -> wcphlex::DriftPlanes  (suffix "drift_planes")
//
// Both outputs are at the input's layer (normally "job") with the module label
// as creator.  Config:
//
//   input:      { creator, layer, suffix }  the GDML product (suffix default "gdml")
//   readout:    "crp"       (wire_cell_phlex/GdmlWires.hpp)
//   sorter:     "cru60d" | "cru60d-pdvd"
//   grouping:   "pair-tpc"
//   min_drift_fraction: 0.1  (shorter drift volumes are insensitive faces)
//   correction: 4           WireSchema correction level

#include "modules/register_shapes.hpp"  // input_selector

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/GdmlWires.hpp"

#include "gdml/objects.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::gdmlwires::Options opt;
    opt.readout = config.get<std::string>("readout", std::string{opt.readout});
    opt.sorter = config.get<std::string>("sorter", std::string{opt.sorter});
    opt.grouping = config.get<std::string>("grouping", std::string{opt.grouping});
    opt.min_drift_fraction = config.get<double>("min_drift_fraction", double{opt.min_drift_fraction});
    opt.correction = config.get<int>("correction", int{opt.correction});

    auto const in = config.get<phlex::configuration>("input");

    m.transform(
         "wcph_gdml_wire_schema",
         [opt](gdml::DocumentPtr const& doc) -> wcphlex::WireSchema {
             return wcphlex::WireSchema{wcphlex::gdmlwires::wire_schema(*doc, opt)};
         },
         phlex::concurrency::unlimited)
        .input_family(wcphlex::input_selector(in, "gdml"))
        .output_product_suffixes("wire_schema");

    m.transform(
         "wcph_gdml_drift_planes",
         [opt](gdml::DocumentPtr const& doc) -> wcphlex::DriftPlanes {
             return wcphlex::DriftPlanes{
                 std::make_shared<const wcphlex::gdmlwires::DriftPlanes>(wcphlex::gdmlwires::drift_planes(*doc, opt))};
         },
         phlex::concurrency::unlimited)
        .input_family(wcphlex::input_selector(in, "gdml"))
        .output_product_suffixes("drift_planes");
}
