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

// modules/frame_source.cpp
//
// PHLEX source module: produces synthetic wcphlex::Frame objects for testing.
//
// For each data cell in the configured layer, creates a SimpleFrame whose ident
// equals data_cell_index::number().  This is the PHLEX-side entry point that
// feeds frames into a downstream wcp_frame_filter module.
//
// Expected config keys:
//   output_layer  (string, required): PHLEX layer name for the output Frame product.

#include "wire_cell_phlex/Data.h"

#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/source.hpp"

#include <WireCellAux/SimpleFrame.h>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(m, config)
{
    auto const layer = config.get<std::string>("output_layer");

    m.provide("wcp_provide_frame",
              [](data_cell_index const& id) -> wcphlex::Frame {
                  auto frame = std::make_shared<WireCell::Aux::SimpleFrame>(
                      static_cast<int>(id.number()));
                  return wcphlex::Frame{std::move(frame)};
              })
      .output_product({.creator = "input", .layer = layer, .suffix = "frame"});
}
