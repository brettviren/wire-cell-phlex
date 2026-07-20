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

// modules/deposet_filter.cpp
//
// PHLEX algorithm module: wraps wcphlex::DepoSetFilter as a PHLEX transform.
//
// The WCT sub-graph (e.g. deposet-passthrough.jsonnet or deposet-drifter.jsonnet)
// is initialized once; each PHLEX event calls operator()(DepoSet) which fills
// the DepoSetBoundarySource, runs the graph, and drains the DepoSetBoundarySink.
//
// Config schema: DepoSetFilterConfig (wire_cell_phlex/Config.hpp) =
//   { phlex: PhlexAlgorithmConfig, executor: ExecutorConfig }
// The `phlex` block carries the generic registration data (name, concurrency,
// input families, output suffixes); `executor` carries the WCT settings.  This
// is the first node migrated to the generic phlex_config schema; the input
// selectors and output suffixes come from config rather than being hard-coded.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Executor.hpp"
#include "wire_cell_phlex/Config_json.hpp"   // value_to<ExecutorConfig>

#include "modules/phlex_adapt.hpp"
#include "boost_config/discovery.hpp"
#include "phlex/module.hpp"

#include <memory>
#include <stdexcept>
#include <string>

using namespace phlex;

// Advertise this node's config schema for CLI discovery (boost-config):
//   scan the plugin's dynamic symbols for the boost_config_factories__ prefix.
BOOST_CONFIG_EXPORT(DepoSetFilterConfig, wcphlex::DepoSetFilterConfig)

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    // Generic Phlex registration data (name / concurrency / inputs / outputs).
    auto const pac = config.get<phlex_config::PhlexAlgorithmConfig>("phlex");

    // WCT executor config; fold in the framework-injected module_label so
    // multi-instance WCT component names stay unique.
    auto exec = config.get<wcphlex::ExecutorConfig>("executor");
    if (auto const ml = config.get_if_present<std::string>("module_label")) {
        if (std::string(exec.module_label).empty()) {
            exec.module_label = *ml;
        }
    }
    auto dsf = std::make_shared<wcphlex::DepoSetFilter>(exec);

    // DepoSetFilter is a 1-input / 1-output transform.
    auto const& inputs = pac.inputs.value;
    auto const& outputs = pac.outputs.value;
    if (inputs.size() != 1 || outputs.size() != 1) {
        throw std::runtime_error(
            "wcph_deposet_filter: expected exactly 1 input selector and 1 output suffix");
    }

    m.transform(std::string(pac.name),
                [dsf](wcphlex::DepoSet const& input) -> wcphlex::DepoSet {
                    return (*dsf)(input);
                },
                wcphlex::to_concurrency(std::string(pac.concurrency)))
      .input_family(wcphlex::to_selector(inputs[0]))
      .output_product_suffixes(outputs[0]);
}
