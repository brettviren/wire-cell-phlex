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

// modules/frame_gen.cpp
//
// PHLEX source module: produces synthetic wcphlex::Frame objects for testing.
//
// For each data cell in the configured layer, creates a SimpleFrame whose ident
// equals data_cell_index::number().  This is the PHLEX-side entry point that
// feeds frames into a downstream wcph_frame_filter module.
//
// Expected config keys:
//   output_layer   (string, required): PHLEX layer name for the output Frame product.
//   output_suffix  (string, optional, default "frame"): product suffix.
//                  Set to a distinct value (e.g. "frame_a") when two instances of
//                  this module run in the same layer so their products don't collide.

#include "wire_cell_phlex/Data.hpp"

#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/source.hpp"

#include <WireCellAux/SimpleFrame.h>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(m, config)
{
    auto const layer  = config.get<std::string>("output_layer");
    auto const suffix = config.get<std::string>("output_suffix", std::string{"frame"});

    m.provide("wcph_provide_frame",
              [](data_cell_index const& id) -> wcphlex::Frame {
                  auto frame = std::make_shared<WireCell::Aux::SimpleFrame>(
                      static_cast<int>(id.number()));
                  return wcphlex::Frame{std::move(frame)};
              })
      .output_product("input", experimental::identifier{suffix}, experimental::identifier{layer});
}
