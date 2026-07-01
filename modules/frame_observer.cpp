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

// modules/frame_observer.cpp
//
// PHLEX observe module: asserts that each received wcphlex::Frame is non-null.
// Useful as an end-cap in test workflows to confirm that a processing module
// (e.g. wcp_frame_filter) produced a valid output Frame for every event.
//
// Expected config keys:
//   input_layer  (string, required): PHLEX layer of the Frame product to consume.
//   input_from   (string, required): creator name of the Frame product
//                (the module label of the upstream transform, e.g. "sigproc_a").

#include "wire_cell_phlex/Data.hpp"

#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

#undef NDEBUG
#include <cassert>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer     = config.get<std::string>("input_layer");
    auto const from      = config.get<std::string>("input_from");

    m.observe("wcp_observe_frame",
              [](wcphlex::Frame const& f) {
                  assert(f.ptr && "observed Frame must be non-null");
              },
              concurrency::serial)
      .input_family(product_selector{.creator = from, .layer = layer, .suffix = "frame"});
}
