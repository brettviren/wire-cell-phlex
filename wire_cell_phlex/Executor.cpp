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

// wire_cell_phlex/Executor.cpp
//
// Implementation of Executor base and FrameFilter / DepoSetToFrame subclasses.
//
// One-time initialization model (mirrors larwirecell WCLS):
//   - Executor() does common config parsing: add_config, add_plugin, tla_var.
//   - Each subclass constructor adds add_app("type:name") and the boundary-node
//     TLAs, then calls m_wcmain.initialize() once.
//   - After initialize(), boundary nodes are found in the WCT factory and
//     stored as raw pointers (factory owns them for the Executor lifetime).
//   - Each operator() call: fill source → m_wcmain() → drain sink.
//   - BoundarySource's queue design allows re-driving the same Pgraph graph
//     across successive events without re-initialization.

#include "wire_cell_phlex/Executor.h"
#include "wire_cell_phlex/Data.h"
#include "wire_cell_phlex/BoundarySource.h"
#include "wire_cell_phlex/BoundarySink.h"

#include <WireCellUtil/NamedFactory.h>

#include <stdexcept>
#include <string>

namespace {

// Look up a boundary node in the WCT factory and downcast it.
// Returns a raw pointer; the factory retains ownership for the duration of
// the WireCell::Main instance that created it.
template <typename Iface, typename Concrete>
Concrete* find_boundary(std::string const& classname, std::string const& instname)
{
    auto iface = WireCell::Factory::find_maybe<Iface>(classname, instname);
    if (!iface) {
        throw std::runtime_error(
            "Executor: WCT factory has no instance " +
            classname + ":" + instname +
            " — check that the Jsonnet config creates it with the correct name");
    }
    auto* raw = dynamic_cast<Concrete*>(iface.get());
    if (!raw) {
        throw std::runtime_error(
            "Executor: dynamic_cast to concrete boundary type failed for " +
            classname + ":" + instname);
    }
    return raw;
}

} // anonymous namespace

namespace wcphlex {

// ---------------------------------------------------------------------------
// Executor base — common config parsing
// ---------------------------------------------------------------------------

Executor::Executor(boost::json::object const& config)
{
    // Required: Jsonnet config file path.
    m_wcmain.add_config(std::string{config.at("wct_config").as_string()});

    // Optional: WCT app type (stored; subclass builds "type:name" for add_app).
    if (config.contains("wct_app")) {
        m_app_type = std::string{config.at("wct_app").as_string()};
    }

    // Always register wire_cell_phlex as a WCT plugin so WIRECELL_FACTORY
    // registrations (BoundarySource, BoundarySink) are findable by the WCT
    // plugin-based factory lookup (NamedFactory falls back to PluginManager
    // when a classname is not in m_lookup, searching all loaded plugins for
    // make_<ClassName>_factory symbols).
    m_wcmain.add_plugin("wire_cell_phlex");

    // Optional: additional WCT plugins (e.g. "WireCellPgraph").
    if (config.contains("wct_plugins")) {
        for (auto const& v : config.at("wct_plugins").as_array()) {
            m_wcmain.add_plugin(std::string{v.as_string()});
        }
    }

    // Optional: user-supplied Jsonnet TLAs.
    if (config.contains("wct_tla")) {
        for (auto const& [k, v] : config.at("wct_tla").as_object()) {
            m_wcmain.tla_var(std::string{k}, std::string{v.as_string()});
        }
    }
    // Note: add_app() and initialize() are called by subclass constructors
    // after they inject their boundary-node TLAs.
}

// ---------------------------------------------------------------------------
// FrameFilter
// ---------------------------------------------------------------------------

FrameFilter::FrameFilter(boost::json::object const& config)
    : Executor(config)
{
    // Inject boundary-node instance names as Jsonnet TLAs.
    m_wcmain.tla_var("source_name", k_source_name);
    m_wcmain.tla_var("sink_name",   k_sink_name);
    m_wcmain.tla_var("app_name",    k_app_name);

    // Register the Pgrapher application by "type:name".
    m_wcmain.add_app(m_app_type + ":" + k_app_name);

    // Initialize the WCT graph (evaluates Jsonnet, creates/configures all
    // components, builds the Pgrapher edge list).
    m_wcmain.initialize();

    // Find the boundary nodes that initialize() just created in the factory.
    m_source = find_boundary<WireCell::IFrameSource,
                             BoundarySource<WireCell::IFrameSource>>(
                   "FrameBoundarySource", k_source_name);

    m_sink = find_boundary<WireCell::IFrameSink,
                           BoundarySink<WireCell::IFrameSink>>(
                 "FrameBoundarySink", k_sink_name);
}

Frame FrameFilter::operator()(Frame const& input)
{
    m_source->fill(input.ptr);   // enqueue data for this event
    m_wcmain();                  // run Pgrapher until quiescent
    return Frame{m_sink->drain()};
}

// ---------------------------------------------------------------------------
// DepoSetToFrame
// ---------------------------------------------------------------------------

DepoSetToFrame::DepoSetToFrame(boost::json::object const& config)
    : Executor(config)
{
    m_wcmain.tla_var("source_name", k_source_name);
    m_wcmain.tla_var("sink_name",   k_sink_name);
    m_wcmain.tla_var("app_name",    k_app_name);

    m_wcmain.add_app(m_app_type + ":" + k_app_name);
    m_wcmain.initialize();

    m_source = find_boundary<WireCell::IDepoSetSource,
                             BoundarySource<WireCell::IDepoSetSource>>(
                   "DepoSetBoundarySource", k_source_name);

    m_sink = find_boundary<WireCell::IFrameSink,
                           BoundarySink<WireCell::IFrameSink>>(
                 "FrameBoundarySink", k_sink_name);
}

Frame DepoSetToFrame::operator()(DepoSet const& input)
{
    m_source->fill(input.ptr);   // enqueue data for this event
    m_wcmain();                  // run Pgrapher until quiescent
    return Frame{m_sink->drain()};
}

} // namespace wcphlex
