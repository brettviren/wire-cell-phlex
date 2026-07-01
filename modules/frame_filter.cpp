/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 */

// modules/frame_filter.cpp
//
// PHLEX algorithm module: wraps wcphlex::FrameFilter as a PHLEX transform.
//
// The FrameFilter (Executor subclass) initializes WireCell::Main ONCE on the
// first event, then processes each PHLEX event by:
//   1. Filling the WCT BoundarySource queue with the input Frame pointer.
//   2. Running the WCT Pgraph sub-graph until quiescent.
//   3. Draining the WCT BoundarySink and returning the result as a wcphlex::Frame.
//
// The WCT sub-graph is configured by a Jsonnet file whose path is supplied via
// the wct_config parameter.  Boundary-node names and the Pgrapher instance name
// are injected as Jsonnet TLAs by the Executor base class.
//
// Expected config keys:
//   wct_config      (string, required): Path to the WCT Jsonnet config file.
//   input_layer     (string, required): PHLEX layer for the input Frame product.
//   input_from      (string, optional, default "input"): PHLEX creator name of
//                   the input Frame product.  "input" (the default) consumes from
//                   a PHLEX source.  Set to the module label of an upstream module
//                   (e.g. "frame_sim") to consume from that module's output Frame.
//   input_suffix    (string, optional, default "frame"): suffix of input product.
//                   Set to a distinct value when consuming one of several frame
//                   streams in the same layer (multi-instance scenario).
//   use_wire_schema (bool, optional, default false): when true, also consume a
//                   wcphlex::WireSchema product from the job layer.  The geometry
//                   is deposited in FacadeWireSchema's static registry before
//                   WCT is initialized, enabling configure()-time IWireSchema
//                   lookups (e.g. by AnodePlane) to succeed.
//   wire_schema_layer (string, optional, default "job"): PHLEX layer from which
//                   to consume the WireSchema product.  Ignored unless
//                   use_wire_schema is true.
//   wct_plugins     (array of strings, optional): Extra WCT plugin libraries.
//   wct_app         (string, optional): WCT IApplication type (default "Pgrapher").
//   wct_tla         (object, optional): String→string map of extra Jsonnet TLAs.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Executor.hpp"

#include "modules/executor_config.hpp"
#include "phlex/module.hpp"

#include <memory>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer        = config.get<std::string>("input_layer");
    auto const from         = config.get<std::string>("input_from", std::string{"input"});
    auto const suffix       = config.get<std::string>("input_suffix", std::string{"frame"});
    auto const use_ws       = config.get<bool>("use_wire_schema", false);
    auto const ws_layer     = config.get<std::string>("wire_schema_layer", std::string{"job"});

    auto ff = std::make_shared<wcphlex::FrameFilter>(to_executor_config(config));

    if (use_ws) {
        // Geometry-aware path: consume WireSchema from job layer + Frame from
        // event layer.  The two-argument operator() deposits the store in
        // FacadeWireSchema's static registry before the first initialize().
        m.transform("wct_frame_filter",
                    [ff](wcphlex::WireSchema const& ws,
                         wcphlex::Frame const& input) -> wcphlex::Frame {
                        return (*ff)(ws, input);
                    },
                    concurrency::serial)
          .input_family(
              product_selector{.creator = "input", .layer = ws_layer,
                            .suffix  = experimental::identifier{"wire_schema"}},
              product_selector{.creator = from, .layer = layer,
                            .suffix  = experimental::identifier{suffix}})
          .output_product_suffixes("frame");
    } else {
        // Plain path: no geometry product consumed.
        m.transform("wct_frame_filter",
                    [ff](wcphlex::Frame const& input) -> wcphlex::Frame {
                        return (*ff)(input);
                    },
                    concurrency::serial)
          .input_family(product_selector{.creator = from, .layer = layer,
                                      .suffix  = experimental::identifier{suffix}})
          .output_product_suffixes("frame");
    }
}
