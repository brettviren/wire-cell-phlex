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
// The FrameFilter (Executor subclass) initializes WireCell::Main ONCE when
// the module is registered, then processes each PHLEX event by:
//   1. Filling the WCT BoundarySource queue with the input Frame pointer.
//   2. Running the WCT Pgraph sub-graph until quiescent.
//   3. Draining the WCT BoundarySink and returning the result as a wcphlex::Frame.
//
// The WCT sub-graph is configured by a Jsonnet file whose path is supplied via
// the wct_config parameter.  Boundary-node names and the Pgrapher instance name
// are injected as Jsonnet TLAs by the Executor base class.
//
// Expected config keys:
//   wct_config   (string, required):    Path to the WCT Jsonnet config file.
//   input_layer  (string, required):    PHLEX layer for the input Frame product.
//   wct_plugins  (array of strings, optional): Extra WCT plugin libraries to load.
//   wct_app      (string, optional):    WCT IApplication type (default "Pgrapher").
//   wct_tla      (object, optional):    String→string map of extra Jsonnet TLAs.

#include "wire_cell_phlex/Data.h"
#include "wire_cell_phlex/Executor.h"

#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

#include <boost/json.hpp>

#include <memory>

using namespace phlex;

namespace {

// Convert phlex::configuration to boost::json::object for the Executor constructor.
// Only the keys known to Executor are extracted; all others are ignored.
boost::json::object to_executor_config(phlex::configuration const& cfg)
{
    boost::json::object obj;

    // Required: WCT Jsonnet config file.
    obj["wct_config"] = cfg.get<std::string>("wct_config");

    // Optional: WCT IApplication type.
    if (auto v = cfg.get_if_present<std::string>("wct_app")) {
        obj["wct_app"] = *v;
    }

    // Optional: additional WCT plugin libraries.
    if (auto v = cfg.get_if_present<std::vector<std::string>>("wct_plugins")) {
        boost::json::array arr;
        for (auto const& s : *v) { arr.push_back(boost::json::value{boost::json::string{s}}); }
        obj["wct_plugins"] = std::move(arr);
    }

    // Optional: extra Jsonnet TLAs (nested string→string object).
    if (auto tla_cfg = cfg.get_if_present<phlex::configuration>("wct_tla")) {
        boost::json::object tla_obj;
        for (auto const& k : tla_cfg->keys()) {
            tla_obj[k] = tla_cfg->get<std::string>(k);
        }
        obj["wct_tla"] = std::move(tla_obj);
    }

    return obj;
}

} // anonymous namespace

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer = config.get<std::string>("input_layer");

    // Construct FrameFilter once; shared_ptr captures it for the transform lambda.
    // WireCell::Main initialization (Jsonnet parse, component creation, graph build)
    // happens here — once per module registration, not per event.
    auto ff = std::make_shared<wcphlex::FrameFilter>(to_executor_config(config));

    // Register a serial transform: WCT graphs are not thread-safe, so concurrent
    // calls to FrameFilter::operator() are disallowed.
    m.transform("wct_frame_filter",
                [ff](wcphlex::Frame const& input) -> wcphlex::Frame {
                    return (*ff)(input);
                },
                concurrency::serial)
      .input_family(product_query{.creator = "input", .layer = layer, .suffix = "frame"})
      .output_product_suffixes("frame");
}
