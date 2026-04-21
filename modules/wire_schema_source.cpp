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

// modules/wire_schema_source.cpp
//
// PHLEX job-layer source module: loads a WCT wire geometry file once per job
// and produces a wcphlex::WireSchema product.
//
// The file is resolved through WireCell::Persist::resolve(), which searches
// directories listed in WIRECELL_PATH, so callers may pass either an absolute
// path or a bare filename that lives somewhere on the path.
//
// This module does NOT create a WireCell::Main instance; it calls
// WireCell::WireSchema::load() directly, which only requires WIRECELL_PATH
// to be set (for path resolution) and is independent of the WCT graph machinery.
//
// Expected config keys:
//   output_layer     (string, required): PHLEX layer at which to publish the
//                    WireSchema product.  Typically "job".
//   wire_schema_file (string, required): Path or filename (resolved via
//                    WIRECELL_PATH) of a WCT wire geometry file
//                    (e.g. "microboone-celltree-wires-v2.1.json.bz2").

#include "wire_cell_phlex/Data.h"

#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/source.hpp"

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(m, config)
{
    auto const layer    = config.get<std::string>("output_layer");
    auto const filename = config.get<std::string>("wire_schema_file");

    m.provide("wcp_provide_wire_schema",
              [filename](data_cell_index const&) -> wcphlex::WireSchema {
                  return wcphlex::WireSchema{WireCell::WireSchema::load(filename.c_str())};
              })
      .output_product({.creator = "input", .layer = layer, .suffix = "wire_schema"});
}
