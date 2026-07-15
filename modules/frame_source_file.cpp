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

// modules/frame_source_file.cpp
//
// PHLEX provider module: reads wcphlex::Frame objects from a WCT frame file.
//
// On the first PHLEX event the entire WCT sub-graph is run to completion,
// queuing all frames in a FrameBoundarySink.  Subsequent events drain one
// Frame per call from that queue.
//
// The PHLEX workflow must set `total` to match the number of frames in the
// input file (e.g. total: 1 for sim-frames.npz from the second real job).
//
// Expected config keys:
//   wct_config   (string, required): Path to frame-file-source.jsonnet (or similar).
//   output_layer (string, required): PHLEX layer name for the output Frame product.
//   wct_plugins  (array of strings, optional): Extra WCT plugin libraries.
//                Must include 'WireCellPgraph' and 'WireCellSio'.
//   wct_tla      (object, optional): Extra Jsonnet TLAs.
//                Use { inname: "path/to/frames.npz" } to set the input file.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Executor.hpp"

#include "modules/executor_config.hpp"
#include "boost_config/discovery.hpp"
#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/source.hpp"

#include <memory>

using namespace phlex;

// Advertise this node's config schema for CLI discovery (boost-config):
//   scan the plugin's dynamic symbols for the boost_config_factories__ prefix.
BOOST_CONFIG_EXPORT(FrameSourceFileConfig, wcphlex::FrameSourceFileConfig)

PHLEX_REGISTER_PROVIDERS(m, config)
{
    auto const layer = config.get<std::string>("output_layer");

    auto src = std::make_shared<wcphlex::FrameSourceFile>(to_executor_config(config));

    m.provide("wct_frame_source_file",
              [src](data_cell_index const&) -> wcphlex::Frame {
                  return (*src)();
              })
      .output_product("input", "frame", experimental::identifier{layer});
}
