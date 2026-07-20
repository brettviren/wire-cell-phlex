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

// modules/two_frame_observer.cpp
//
// PHLEX observe module: receives two wcphlex::Frame products from two different
// upstream modules and asserts both are non-null and distinct from each other.
//
// Used in the multi-instance workflow (Step 7) to verify that two independent
// wcph_frame_filter instances produce separate, valid output streams.
//
// Expected config keys:
//   input_layer   (string, required): PHLEX layer for both Frame products.
//   input_from_a  (string, required): creator name of the first Frame product.
//   input_from_b  (string, required): creator name of the second Frame product.

#include "wire_cell_phlex/Data.hpp"

#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

#undef NDEBUG
#include <cassert>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer  = config.get<std::string>("input_layer");
    auto const from_a = config.get<std::string>("input_from_a");
    auto const from_b = config.get<std::string>("input_from_b");

    m.observe("wcph_observe_two_frames",
              [](wcphlex::Frame const& fa, wcphlex::Frame const& fb) {
                  assert(fa.ptr && "first observed Frame must be non-null");
                  assert(fb.ptr && "second observed Frame must be non-null");
                  assert(fa.ptr != fb.ptr &&
                         "frames from independent module instances must be distinct");
              },
              concurrency::serial)
      .input_family(
          product_selector{.creator = from_a, .layer = layer, .suffix = "frame"},
          product_selector{.creator = from_b, .layer = layer, .suffix = "frame"});
}
