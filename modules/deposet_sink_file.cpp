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

// modules/deposet_sink_file.cpp
//
// PHLEX observer module: writes each received wcphlex::DepoSet to a WCT depo file.
//
// Each PHLEX event delivers one DepoSet to the WCT DepoFileSink via a
// DepoSetBoundarySource.  The file is flushed and closed when the
// DepoSetSinkFile executor is destroyed (i.e. after the PHLEX job ends).
//
// Expected config keys:
//   wct_config   (string, required): Path to deposet-file-sink.jsonnet (or similar).
//   input_layer  (string, required): PHLEX layer of the input DepoSet product.
//   input_from   (string, required): creator name of the upstream DepoSet product.
//   wct_plugins  (array of strings, optional): Extra WCT plugin libraries.
//                Must include 'WireCellPgraph' and 'WireCellSio'.
//   wct_tla      (object, optional): Extra Jsonnet TLAs.
//                Use { outname: "path/to/output.npz" } to set the output file.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Executor.hpp"

#include "modules/executor_config.hpp"
#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

#include <memory>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer = config.get<std::string>("input_layer");
    auto const from  = config.get<std::string>("input_from");

    auto snk = std::make_shared<wcphlex::DepoSetSinkFile>(to_executor_config(config));

    m.observe("wct_deposet_sink_file",
              [snk](wcphlex::DepoSet const& ds) {
                  (*snk)(ds);
              },
              concurrency::serial)
      .input_family(product_selector{.creator = from, .layer = layer, .suffix = "deposet"});
}
