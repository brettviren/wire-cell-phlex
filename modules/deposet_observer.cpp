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

// modules/deposet_observer.cpp
//
// PHLEX observe module: asserts that each received wcphlex::DepoSet is non-null.
// Useful as an end-cap in test workflows to confirm that a processing module
// (e.g. wcp_deposet_filter) produced a valid output DepoSet for every event.
//
// Expected config keys:
//   input_layer  (string, required): PHLEX layer of the DepoSet product to consume.
//   input_from   (string, required): creator name of the DepoSet product
//                (the module label of the upstream transform, e.g. "deposet_filter").

#include "wire_cell_phlex/Data.hpp"

#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

#undef NDEBUG
#include <cassert>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer = config.get<std::string>("input_layer");
    auto const from  = config.get<std::string>("input_from");

    m.observe("wcp_observe_deposet",
              [](wcphlex::DepoSet const& ds) {
                  assert(ds.ptr && "observed DepoSet must be non-null");
              },
              concurrency::serial)
      .input_family(product_selector{.creator = from, .layer = layer, .suffix = "deposet"});
}
