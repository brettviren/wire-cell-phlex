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

// modules/deposet_gen.cpp
//
// PHLEX source module: produces synthetic wcphlex::DepoSet objects for testing.
//
// For each data cell in the configured layer, creates an empty SimpleDepoSet
// whose ident equals data_cell_index::number().  This feeds a downstream
// wcph_deposet_to_frame module.
//
// Expected config keys:
//   output_layer  (string, required): PHLEX layer name for the output DepoSet product.

#include "wire_cell_phlex/Data.hpp"

#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/source.hpp"

#include <WireCellAux/SimpleDepoSet.h>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(m, config)
{
    auto const layer = config.get<std::string>("output_layer");

    m.provide("wcph_provide_deposet",
              [](data_cell_index const& id) -> wcphlex::DepoSet {
                  auto ds = std::make_shared<WireCell::Aux::SimpleDepoSet>(
                      static_cast<int>(id.number()),
                      WireCell::IDepo::vector{});
                  return wcphlex::DepoSet{std::move(ds)};
              })
      .output_product("input", "deposet", experimental::identifier{layer});
}
