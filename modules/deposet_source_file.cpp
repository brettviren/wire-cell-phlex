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

// modules/deposet_source_file.cpp
//
// PHLEX provider module: reads wcphlex::DepoSet objects from a WCT depo file.
//
// On the first PHLEX event the entire WCT sub-graph is run to completion,
// queuing all depo sets in a DepoSetBoundarySink.  Subsequent events drain
// one depo set per call from that queue.
//
// The PHLEX workflow must set `total` to match the number of depo sets in the
// input file (e.g. total: 1 for muon-depos.npz which contains one depo set).
//
// Expected config keys:
//   wct_config   (string, required): Path to deposet-file-source.jsonnet (or similar).
//   output_layer (string, required): PHLEX layer name for the output DepoSet product.
//   wct_plugins  (array of strings, optional): Extra WCT plugin libraries.
//                Must include 'WireCellPgraph' and 'WireCellSio'.
//   wct_tla      (object, optional): Extra Jsonnet TLAs.
//                Use { inname: "path/to/depos.npz" } to set the input file.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Executor.hpp"

#include "modules/executor_config.hpp"
#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/source.hpp"

#include <memory>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(m, config)
{
    auto const layer = config.get<std::string>("output_layer");

    auto src = std::make_shared<wcphlex::DepoSetSourceFile>(to_executor_config(config));

    m.provide("wct_deposet_source_file",
              [src](data_cell_index const&) -> wcphlex::DepoSet {
                  return (*src)();
              })
      .output_product("input", "deposet", experimental::identifier{layer});
}
