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

// modules/wire_schema_observer.cpp
//
// PHLEX observe module: validates a wcphlex::WireSchema product by asserting
// that it was successfully loaded (non-null internal store) and contains at
// least one wire.
//
// Intended for use at the job layer to verify that geometry was loaded before
// any event processing begins.
//
// Expected config keys:
//   input_layer  (string, required): PHLEX layer of the WireSchema product.
//                Typically "job".

#include "wire_cell_phlex/Data.h"

#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

#undef NDEBUG
#include <cassert>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer = config.get<std::string>("input_layer");

    m.observe("wcp_observe_wire_schema",
              [](wcphlex::WireSchema const& ws) {
                  assert(ws.store.db() && "WireSchema store must be loaded (non-null db)");
                  assert(!ws.store.wires().empty() &&
                         "WireSchema must contain at least one wire");
              },
              concurrency::serial)
      .input_family(product_query{.creator = "input", .layer = layer, .suffix = "wire_schema"});
}
