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

#pragma once

// modules/register_shapes.hpp
//
// Concise PHLEX registration for the templated shape executors, following the
// module naming convention: a WCT IData type is named by its <type> token (the
// "I" removed, lower-cased) and a function node by <in>_to_<out>.  The node
// name given to m.transform is "wcph_<name>".
//
// These helpers keep each module's body to a single call, e.g.
//   PHLEX_REGISTER_ALGORITHMS(m, config) {
//       wcphlex::register_function<WireCell::IDepoSet, WireCell::IFrame>(m, config);
//   }

#include "wire_cell_phlex/ShapeExecutors.hpp"
#include "wire_cell_phlex/Config_json.hpp"   // value_to<ExecutorConfig>

#include "modules/executor_config.hpp"        // to_executor_config

#include "phlex/module.hpp"

#include <boost/json.hpp>

#include <memory>
#include <string>

namespace wcphlex {

// Build the WCT ExecutorConfig for a module from its (flat) config keys,
// folding in the framework-injected module_label so multi-instance WCT
// component names stay unique.
inline ExecutorConfig executor_config_from(phlex::configuration const& config)
{
    auto obj = to_executor_config(config);   // { "executor": {...} }
    auto ec = boost::json::value_to<ExecutorConfig>(obj.at("executor"));
    if (auto const ml = config.get_if_present<std::string>("module_label")) {
        if (std::string(ec.module_label).empty()) {
            ec.module_label = *ml;
        }
    }
    return ec;
}

// Register a 1->1 function node (FunctionExecutor<In,Out>).  Node name
// "wcph_<in>_to_<out>".  The input creator must be named explicitly via the
// required "input_from" config key: the literal "input" to consume a source's
// output, or an upstream module's label to chain off it.  The product suffixes
// default to the type <stem>s (input <in>, output <out>) but may be overridden
// via "input_suffix" / "output_suffix" — e.g. to disambiguate several streams
// of the same type in one layer (multi-instance).
template <class In, class Out, class Proxy>
void register_function(Proxy& m, phlex::configuration const& config)
{
    const std::string layer = config.get<std::string>("input_layer");
    const std::string from = config.get<std::string>("input_from");
    const std::string in_suffix = config.get<std::string>("input_suffix", type_stem<In>());
    const std::string out_suffix = config.get<std::string>("output_suffix", type_stem<Out>());

    auto node = std::make_shared<FunctionExecutor<In, Out>>(executor_config_from(config));

    m.transform("wcph_" + type_stem<In>() + "_to_" + type_stem<Out>(),
                [node](Data<In> const& in) -> Data<Out> { return (*node)(in); },
                phlex::concurrency::serial)
        .input_family(phlex::product_selector{
            .creator = from, .layer = layer,
            .suffix = phlex::experimental::identifier{in_suffix}})
        .output_product_suffixes(out_suffix);
}

} // namespace wcphlex
