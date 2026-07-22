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
// PHLEX algorithm module: IFrame -> IFrame function node.
//
// Two paths share one library:
//
//   * Plain (default): a 1-in/1-out FunctionExecutor<IFrame,IFrame> registered
//     via register_function() — data crosses the Phlex boundary through
//     GenericFrameBoundarySource / GenericFrameBoundarySink.
//
//   * Auxiliary wire-schema (use_wire_schema: true): a 2-input transform on the
//     original FrameFilter executor that also consumes a WireSchema product and
//     deposits it in FacadeWireSchema's registry before WCT initialize(), so
//     configure()-time IWireSchema lookups (e.g. by AnodePlane) succeed.  This
//     is the deferred "auxiliary input" shape; it remains hand-written until
//     the shape family grows a generic aux-input mechanism.
//
// Config keys: wct_config (required), input_layer (required), input_from
// (required: "input" to consume a source, else an upstream module label),
// input_suffix / output_suffix (optional, default "frame"),
// use_wire_schema (bool, optional, default false),
// wire_schema_layer (string, optional, default "job"; used only when
// use_wire_schema is true), wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Executor.hpp"   // FrameFilter (wire-schema path)

#include "modules/register_shapes.hpp"
#include "modules/executor_config.hpp"    // to_executor_config (wire-schema path)

#include "phlex/module.hpp"

#include <memory>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    if (!config.get<bool>("use_wire_schema", false)) {
        // Plain IFrame -> IFrame transform on the templated executor.
        wcphlex::register_function<WireCell::IFrame, WireCell::IFrame>(m, config);
        return;
    }

    // --- Auxiliary wire-schema path (deferred generic aux-input) -------------
    auto const layer    = config.get<std::string>("input_layer");
    auto const from     = config.get<std::string>("input_from");
    auto const suffix   = config.get<std::string>("input_suffix", std::string{"frame"});
    auto const ws_layer = config.get<std::string>("wire_schema_layer", std::string{"job"});

    // Uniform node config shape: { "executor": {...}, "use_wire_schema": true }.
    auto exec_obj = to_executor_config(config);
    exec_obj["use_wire_schema"] = true;
    auto ff = std::make_shared<wcphlex::FrameFilter>(exec_obj);

    // Consume WireSchema (job layer, from a wire_schema source) + Frame.  The
    // two-argument operator() deposits the store in FacadeWireSchema's static
    // registry before the first initialize().
    m.transform("wcph_frame_filter",
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
}
