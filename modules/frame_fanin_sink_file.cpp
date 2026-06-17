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

// modules/frame_fanin_sink_file.cpp
//
// PHLEX observer module: receives 4 wcphlex::Frame products (one per APA),
// merges them via WCT FrameFanin, and writes the merged Frame to a file via
// WCT FrameFileSink.
//
// PHLEX-level fan-in: input_family() with 4 product_queries creates a
// multilayer_join_node<4> that fires only when all 4 Frames for the same
// event have arrived, regardless of order.
//
// WCT-level merge: the WCT sub-graph (frame-fanin-file-sink.jsonnet) has
// 4 FrameBoundarySource nodes feeding FrameFanin (multiplicity=4), whose
// single output goes to FrameFileSink.
//
// Expected config keys:
//   wct_config    (string, required): Path to frame-fanin-file-sink.jsonnet.
//   input_layer   (string, required): PHLEX layer for all 4 Frame products.
//   input_from_0  (string, required): creator name of Frame from APA 0.
//   input_from_1  (string, required): creator name of Frame from APA 1.
//   input_from_2  (string, required): creator name of Frame from APA 2.
//   input_from_3  (string, required): creator name of Frame from APA 3.
//   wct_plugins   (array of strings, optional): Must include WireCellPgraph,
//                 WireCellGen, WireCellSio.
//   wct_tla       (object, optional): Use { outname: "path/to/output.npz" }.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Executor.hpp"

#include "modules/executor_config.hpp"
#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

#include <memory>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer   = config.get<std::string>("input_layer");
    auto const from_0  = config.get<std::string>("input_from_0");
    auto const from_1  = config.get<std::string>("input_from_1");
    auto const from_2  = config.get<std::string>("input_from_2");
    auto const from_3  = config.get<std::string>("input_from_3");

    auto snk = std::make_shared<wcphlex::FrameFaninSinkFile>(to_executor_config(config));

    m.observe("wct_frame_fanin_sink_file",
              [snk](wcphlex::Frame const& f0, wcphlex::Frame const& f1,
                    wcphlex::Frame const& f2, wcphlex::Frame const& f3) {
                  (*snk)(f0, f1, f2, f3);
              },
              concurrency::serial)
      .input_family(
          product_query{.creator = from_0, .layer = layer, .suffix = "frame"},
          product_query{.creator = from_1, .layer = layer, .suffix = "frame"},
          product_query{.creator = from_2, .layer = layer, .suffix = "frame"},
          product_query{.creator = from_3, .layer = layer, .suffix = "frame"});
}
