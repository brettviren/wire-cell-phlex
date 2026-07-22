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
#include "phlex/model/data_cell_index.hpp"    // data_cell_index (register_source)

#include <boost/json.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace wcphlex {

namespace detail {
// Map an index to a fixed type T, for expanding a compile-time-N parameter pack
// (Data<In> const&... / std::tuple<Data<Out>...>) in the fan helpers below.
template <class T, std::size_t>
using repeat = T;
}  // namespace detail

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

// Register a 1->0 sink node (SinkExecutor<In>).  Consumes a Data<In> and drives
// a WCT sub-graph that terminates in a real WCT sink (e.g. a file writer); no
// Phlex product is produced.  Node name "wcph_<in>_sink"; input creator named
// via the required "input_from" key (an upstream module label, or "input" to
// consume a source), input suffix defaulting to the type <stem>.
template <class In, class Proxy>
void register_sink(Proxy& m, phlex::configuration const& config)
{
    const std::string layer = config.get<std::string>("input_layer");
    const std::string from = config.get<std::string>("input_from");
    const std::string in_suffix = config.get<std::string>("input_suffix", type_stem<In>());

    auto node = std::make_shared<SinkExecutor<In>>(executor_config_from(config));

    m.observe("wcph_" + type_stem<In>() + "_sink",
              [node](Data<In> const& in) { (*node)(in); },
              phlex::concurrency::serial)
        .input_family(phlex::product_selector{
            .creator = from, .layer = layer,
            .suffix = phlex::experimental::identifier{in_suffix}});
}

// Register a 0->1 source node (SourceExecutor<Out>).  Drives a WCT sub-graph
// that begins with a real WCT source (e.g. a file reader) and ends in a
// GenericBoundarySink; the first call runs the graph and queues every output,
// each Phlex call drains one.  Registered as a Phlex provider named
// "wcph_<out>_source"; the output product's creator is the literal "input"
// (the source convention) with suffix defaulting to the type <stem>.
//
// NOTE: this is the WCT-graph source shape.  Trivial in-memory test generators
// (SimpleFrame/SimpleDepoSet) are a different, pure-Phlex kind and live in the
// *_gen modules.
template <class Out, class Proxy>
void register_source(Proxy& m, phlex::configuration const& config)
{
    const std::string layer = config.get<std::string>("output_layer");
    const std::string out_suffix = config.get<std::string>("output_suffix", type_stem<Out>());

    auto node = std::make_shared<SourceExecutor<Out>>(executor_config_from(config));

    m.provide("wcph_" + type_stem<Out>() + "_source",
              [node](phlex::data_cell_index const&) -> Data<Out> { return (*node)(); })
        .output_product("input", phlex::experimental::identifier{out_suffix},
                        phlex::experimental::identifier{layer});
}

// ---------------------------------------------------------------------------
// Fans.
//
// Phlex 0.3.2 retrieves exactly one product per input parameter
// (phlex/core/input_arguments.hpp), so a node's port count is its compile-time
// function arity — a product family cannot be gathered into one std::vector
// argument.  A fan's multiplicity is therefore a template constant N on the
// Phlex side (N product ports), which is passed through as the runtime
// multiplicity of the (WCT-dynamic) FaninExecutor / FanoutExecutor.
// ---------------------------------------------------------------------------

// N -> 1 homogeneous fan-in (FaninExecutor<In,Out>).  Node "wcph_<in>s_to_<out>"
// (input type pluralised per the naming convention).  Consumes N products, one
// per required "input_from_0" .. "input_from_<N-1>" creator (all same layer and
// input suffix), packs them into the vector FaninExecutor expects, and produces
// one output.
template <class In, class Out, std::size_t N, class Proxy, std::size_t... Is>
void register_fanin_impl(Proxy& m, phlex::configuration const& config, std::index_sequence<Is...>)
{
    const std::string layer = config.get<std::string>("input_layer");
    const std::string in_suffix = config.get<std::string>("input_suffix", type_stem<In>());
    const std::string out_suffix = config.get<std::string>("output_suffix", type_stem<Out>());
    const std::array<std::string, N> froms{
        config.get<std::string>("input_from_" + std::to_string(Is))...};

    auto node = std::make_shared<FaninExecutor<In, Out>>(executor_config_from(config), N);

    m.transform(std::string("wcph_") + type_stem<In>() + "s_to_" + type_stem<Out>(),
                [node](detail::repeat<Data<In>, Is> const&... ins) -> Data<Out> {
                    return (*node)(std::vector<Data<In>>{ins...});
                },
                phlex::concurrency::serial)
        .input_family(phlex::product_selector{
            .creator = froms[Is], .layer = layer,
            .suffix = phlex::experimental::identifier{in_suffix}}...)
        .output_product_suffixes(out_suffix);
}

template <class In, class Out, std::size_t N, class Proxy>
void register_fanin(Proxy& m, phlex::configuration const& config)
{
    register_fanin_impl<In, Out, N>(m, config, std::make_index_sequence<N>{});
}

// 1 -> N homogeneous fan-out (FanoutExecutor<In,Out>).  Node "wcph_<in>_to_<out>s".
// Consumes one product and produces N, with distinct suffixes "<out>_0" ..
// "<out>_<N-1>" (base overridable via "output_suffix") so they coexist in one
// layer.  The FanoutExecutor's std::vector<Data<Out>> is unpacked into the N-way
// tuple Phlex expects for a multi-output transform.
template <class In, class Out, std::size_t N, class Proxy, std::size_t... Is>
void register_fanout_impl(Proxy& m, phlex::configuration const& config, std::index_sequence<Is...>)
{
    const std::string layer = config.get<std::string>("input_layer");
    const std::string from = config.get<std::string>("input_from");
    const std::string in_suffix = config.get<std::string>("input_suffix", type_stem<In>());
    const std::string out_base = config.get<std::string>("output_suffix", type_stem<Out>());

    auto node = std::make_shared<FanoutExecutor<In, Out>>(executor_config_from(config), N);

    m.transform(std::string("wcph_") + type_stem<In>() + "_to_" + type_stem<Out>() + "s",
                [node](Data<In> const& in) -> std::tuple<detail::repeat<Data<Out>, Is>...> {
                    auto outs = (*node)(in);
                    return std::tuple<detail::repeat<Data<Out>, Is>...>{outs[Is]...};
                },
                phlex::concurrency::serial)
        .input_family(phlex::product_selector{
            .creator = from, .layer = layer,
            .suffix = phlex::experimental::identifier{in_suffix}})
        .output_product_suffixes((out_base + "_" + std::to_string(Is))...);
}

template <class In, class Out, std::size_t N, class Proxy>
void register_fanout(Proxy& m, phlex::configuration const& config)
{
    register_fanout_impl<In, Out, N>(m, config, std::make_index_sequence<N>{});
}

} // namespace wcphlex
