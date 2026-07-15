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

// modules/deposet_to_frame.cpp
//
// PHLEX algorithm module: wraps wcphlex::DepoSetToFrame as a PHLEX transform.
//
// Mirrors frame_filter.cpp but for IDepoSet → IFrame conversion.  The WCT
// sub-graph (e.g. deposet-to-frame.jsonnet) is initialized once; each PHLEX
// event calls operator()(DepoSet) which fills the DepoSetBoundarySource,
// runs the graph, and drains the FrameBoundarySink.
//
// Expected config keys:
//   wct_config   (string, required):    Path to the WCT Jsonnet config file.
//   input_layer  (string, required):    PHLEX layer for the input DepoSet product.
//   wct_plugins  (array of strings, optional): Extra WCT plugin libraries to load.
//   wct_app      (string, optional):    WCT IApplication type (default "Pgrapher").
//   wct_tla      (object, optional):    String→string map of extra Jsonnet TLAs.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Executor.hpp"

#include "modules/executor_config.hpp"
#include "boost_config/discovery.hpp"
#include "phlex/module.hpp"

#include <memory>

using namespace phlex;

// Advertise this node's config schema for CLI discovery (boost-config):
//   scan the plugin's dynamic symbols for the boost_config_factories__ prefix.
BOOST_CONFIG_EXPORT(DepoSetToFrameConfig, wcphlex::DepoSetToFrameConfig)

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer = config.get<std::string>("input_layer");

    auto d2f = std::make_shared<wcphlex::DepoSetToFrame>(to_executor_config(config));

    m.transform("wct_deposet_to_frame",
                [d2f](wcphlex::DepoSet const& input) -> wcphlex::Frame {
                    return (*d2f)(input);
                },
                concurrency::serial)
      .input_family(product_selector{.creator = "input", .layer = layer, .suffix = "deposet"})
      .output_product_suffixes("frame");
}
