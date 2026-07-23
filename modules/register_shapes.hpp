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

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace wcphlex {

namespace detail {
// Map an index to a fixed type T, for expanding a compile-time-N parameter pack
// (Data<In> const&... / std::tuple<Data<Out>...>) in the fan helpers below.
template <class T, std::size_t>
using repeat = T;
}  // namespace detail

// Build the WCT ExecutorConfig for a module from its config keys, folding in the
// framework-injected module_label so multi-instance WCT component names stay
// unique.
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

// ---------------------------------------------------------------------------
// inputs / outputs config arrays.
//
// Every shape reads two array-valued config keys, "inputs" and "outputs", each
// an array of { creator, layer, suffix, stage } objects — the same array shape
// as the WCT-side "sources"/"sinks".  Either array may be empty (source has no
// inputs, sink has no outputs) but both are always present.  The element count
// must equal the shape's port arity.
//
// A transform's output creator is fixed to the node's own (module) label and
// its output layer to the input's layer (Phlex 0.3.2 declared_transform.hpp),
// so a transform "outputs" element may set only "suffix"; setting "creator" or
// "layer" there is a configuration error (option A).  A source (provider)
// output honors all three fields.
// ---------------------------------------------------------------------------

inline std::vector<phlex::configuration> read_ports(phlex::configuration const& config,
                                                    std::string const& key)
{
    return config.get<std::vector<phlex::configuration>>(key);
}

inline void check_arity(std::string const& node,
                        std::size_t n_in, std::size_t want_in,
                        std::size_t n_out, std::size_t want_out)
{
    if (n_in != want_in || n_out != want_out) {
        throw std::runtime_error(
            node + ": expected " + std::to_string(want_in) + " input(s) and " +
            std::to_string(want_out) + " output(s), got " + std::to_string(n_in) + " and " +
            std::to_string(n_out) + " (check the module's 'inputs'/'outputs' config arrays)");
    }
}

// Build an input product_selector from an { creator, layer, suffix?, stage? }
// element.  layer is required; creator is optional (empty/omitted matches any);
// suffix defaults to the port type <stem>; stage is left unset unless given.
inline phlex::product_selector input_selector(phlex::configuration const& e,
                                              std::string const& default_suffix)
{
    phlex::product_selector sel{.layer = e.get<std::string>("layer")};
    if (auto c = e.get_if_present<std::string>("creator"); c && !c->empty()) {
        sel.creator = *c;
    }
    sel.suffix = phlex::experimental::identifier{e.get<std::string>("suffix", std::string{default_suffix})};
    if (auto st = e.get_if_present<std::string>("stage")) {
        sel.stage = phlex::experimental::identifier{*st};
    }
    return sel;
}

// The output suffix of a transform-family node.  Only "suffix" is meaningful
// (defaulting to the port type <stem>); "creator"/"layer" are framework-fixed
// for a transform, so their presence is a configuration error (option A).
inline std::string transform_output_suffix(phlex::configuration const& e,
                                           std::string const& default_suffix)
{
    if (e.get_if_present<std::string>("creator") || e.get_if_present<std::string>("layer")) {
        throw std::runtime_error(
            "wcphlex: a transform 'outputs' element may set only 'suffix' — its creator is fixed "
            "to the node's module label and its layer to the input's layer");
    }
    return e.get<std::string>("suffix", std::string{default_suffix});
}

// Register a 1->1 function node (FunctionExecutor<In,Out>).  Node name
// "wcph_<in>_to_<out>".  Reads one "inputs" selector and one "outputs" element
// (suffixes default to the port type <stem>s).
template <class In, class Out, class Proxy>
void register_function(Proxy& m, phlex::configuration const& config)
{
    const std::string node = "wcph_" + type_stem<In>() + "_to_" + type_stem<Out>();
    auto inputs = read_ports(config, "inputs");
    auto outputs = read_ports(config, "outputs");
    check_arity(node, inputs.size(), 1, outputs.size(), 1);

    auto exec = std::make_shared<FunctionExecutor<In, Out>>(executor_config_from(config));

    m.transform(node,
                [exec](Data<In> const& in) -> Data<Out> { return (*exec)(in); },
                phlex::concurrency::serial)
        .input_family(input_selector(inputs[0], type_stem<In>()))
        .output_product_suffixes(transform_output_suffix(outputs[0], type_stem<Out>()));
}

// Register a 1->0 sink node (SinkExecutor<In>).  Consumes a Data<In> and drives
// a WCT sub-graph that terminates in a real WCT sink (e.g. a file writer); no
// Phlex product is produced.  Node name "wcph_<in>_sink"; one "inputs" selector,
// empty "outputs".
template <class In, class Proxy>
void register_sink(Proxy& m, phlex::configuration const& config)
{
    const std::string node = "wcph_" + type_stem<In>() + "_sink";
    auto inputs = read_ports(config, "inputs");
    auto outputs = read_ports(config, "outputs");
    check_arity(node, inputs.size(), 1, outputs.size(), 0);

    auto exec = std::make_shared<SinkExecutor<In>>(executor_config_from(config));

    m.observe(node,
              [exec](Data<In> const& in) { (*exec)(in); },
              phlex::concurrency::serial)
        .input_family(input_selector(inputs[0], type_stem<In>()));
}

// Register a 0->1 source node (SourceExecutor<Out>).  Drives a WCT sub-graph
// that begins with a real WCT source (e.g. a file reader) and ends in a
// GenericBoundarySink; the first call runs the graph and queues every output,
// each Phlex call drains one.  Registered as a Phlex provider named
// "wcph_<out>_source".  Empty "inputs"; one "outputs" element — a provider
// honors its full { creator, layer, suffix } (creator defaults to the source
// convention "input", suffix to the port type <stem>).
//
// NOTE: this is the WCT-graph source shape.  Trivial in-memory test generators
// (SimpleFrame/SimpleDepoSet) are a different, pure-Phlex kind and live in the
// *_gen modules.
template <class Out, class Proxy>
void register_source(Proxy& m, phlex::configuration const& config)
{
    const std::string node = "wcph_" + type_stem<Out>() + "_source";
    auto inputs = read_ports(config, "inputs");
    auto outputs = read_ports(config, "outputs");
    check_arity(node, inputs.size(), 0, outputs.size(), 1);

    auto const& o = outputs[0];
    const std::string creator = o.get<std::string>("creator", std::string{"input"});
    const std::string layer = o.get<std::string>("layer");
    const std::string suffix = o.get<std::string>("suffix", type_stem<Out>());

    auto exec = std::make_shared<SourceExecutor<Out>>(executor_config_from(config));

    m.provide(node,
              [exec](phlex::data_cell_index const&) -> Data<Out> { return (*exec)(); })
        .output_product(creator, phlex::experimental::identifier{suffix},
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
// per "inputs" selector (each finger its own creator/layer/suffix), packs them
// into the vector FaninExecutor expects, and produces one output.
template <class In, class Out, std::size_t N, class Proxy, std::size_t... Is>
void register_fanin_impl(Proxy& m, phlex::configuration const& config, std::index_sequence<Is...>)
{
    const std::string node = "wcph_" + type_stem<In>() + "s_to_" + type_stem<Out>();
    auto inputs = read_ports(config, "inputs");
    auto outputs = read_ports(config, "outputs");
    check_arity(node, inputs.size(), N, outputs.size(), 1);

    auto exec = std::make_shared<FaninExecutor<In, Out>>(executor_config_from(config), N);

    m.transform(node,
                [exec](detail::repeat<Data<In>, Is> const&... ins) -> Data<Out> {
                    return (*exec)(std::vector<Data<In>>{ins...});
                },
                phlex::concurrency::serial)
        .input_family(input_selector(inputs[Is], type_stem<In>())...)
        .output_product_suffixes(transform_output_suffix(outputs[0], type_stem<Out>()));
}

template <class In, class Out, std::size_t N, class Proxy>
void register_fanin(Proxy& m, phlex::configuration const& config)
{
    register_fanin_impl<In, Out, N>(m, config, std::make_index_sequence<N>{});
}

// 1 -> N homogeneous fan-out (FanoutExecutor<In,Out>).  Node "wcph_<in>_to_<out>s".
// Consumes one product and produces N — one per "outputs" element, which must
// give distinct suffixes so they coexist in one layer.  The FanoutExecutor's
// std::vector<Data<Out>> is unpacked into the N-way tuple Phlex expects for a
// multi-output transform.
template <class In, class Out, std::size_t N, class Proxy, std::size_t... Is>
void register_fanout_impl(Proxy& m, phlex::configuration const& config, std::index_sequence<Is...>)
{
    const std::string node = "wcph_" + type_stem<In>() + "_to_" + type_stem<Out>() + "s";
    auto inputs = read_ports(config, "inputs");
    auto outputs = read_ports(config, "outputs");
    check_arity(node, inputs.size(), 1, outputs.size(), N);

    auto exec = std::make_shared<FanoutExecutor<In, Out>>(executor_config_from(config), N);

    m.transform(node,
                [exec](Data<In> const& in) -> std::tuple<detail::repeat<Data<Out>, Is>...> {
                    auto outs = (*exec)(in);
                    return std::tuple<detail::repeat<Data<Out>, Is>...>{outs[Is]...};
                },
                phlex::concurrency::serial)
        .input_family(input_selector(inputs[0], type_stem<In>()))
        .output_product_suffixes(transform_output_suffix(outputs[Is], type_stem<Out>())...);
}

template <class In, class Out, std::size_t N, class Proxy>
void register_fanout(Proxy& m, phlex::configuration const& config)
{
    register_fanout_impl<In, Out, N>(m, config, std::make_index_sequence<N>{});
}

} // namespace wcphlex
