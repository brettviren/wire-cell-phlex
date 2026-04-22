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
// Deferred initialization model (mirrors larwirecell WCLS):
//   - Executor() does common config parsing: add_config, add_plugin, tla_var.
//   - Each subclass constructor stores boundary-node names and app name, and
//     registers TLAs and add_app() with m_wcmain — but does NOT call initialize().
//   - The first operator() call triggers ensure_initialized(), which calls
//     m_wcmain.initialize() and locates boundary nodes via the WCT factory.
//   - Each operator() call: fill source → m_wcmain() → drain sink.
//   - BoundarySource's queue design allows re-driving the same Pgraph graph
//     across successive events without re-initialization.
//
// Geometry-aware path (FrameFilter):
//   - operator()(WireSchema const&, Frame const&) calls
//     FacadeWireSchema::register_store(m_scope, ws.store) before the first
//     ensure_initialized().  This pre-populates the static map that
//     FacadeWireSchema::configure() reads during initialize().

#include "wire_cell_phlex/Executor.h"
#include "wire_cell_phlex/Data.h"
#include "wire_cell_phlex/BoundarySource.h"
#include "wire_cell_phlex/BoundarySink.h"
#include "wire_cell_phlex/FacadeWireSchema.h"

#include <WireCellUtil/NamedFactory.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {
// Serializes all WireCell::Main::initialize() calls across all Executor instances.
//
// WCT's global NamedFactoryRegistry and PluginManager are not thread-safe:
// concurrent initialize() calls from two different Executor instances (e.g.
// two FrameFilter instances loaded under different PHLEX module labels) corrupt
// the factory.  Before deferred init was introduced, constructors ran sequentially
// at PHLEX registration time, so this was never an issue.  Now that init fires on
// the first event (potentially in parallel TBB tasks), we must serialize.
std::mutex s_wct_init_mutex;
} // anonymous namespace

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
    // Scope prefix for WCT component instance names.  PHLEX injects
    // "module_label" into every module config; use it when present so
    // that two executor instances loaded under different PHLEX module keys
    // create uniquely-named WCT components in the global factory.
    if (config.contains("module_label")) {
        m_scope = std::string{config.at("module_label").as_string()};
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
    // Derive unique WCT component instance names from m_scope so that two
    // FrameFilter instances loaded under different PHLEX module labels do not
    // collide in the global WCT factory.
    m_src_name = m_scope + "_frame_source";
    m_snk_name = m_scope + "_frame_sink";
    m_app_name = m_scope + "_pgrapher";

    // Register TLAs and app now; initialize() is deferred to first operator() call.
    m_wcmain.tla_var("source_name", m_src_name);
    m_wcmain.tla_var("sink_name",   m_snk_name);
    m_wcmain.tla_var("app_name",    m_app_name);

    // When using the FacadeWireSchema path, inject wire_schema_name = m_scope so
    // the Jsonnet config can use it as the FacadeWireSchema instance name and scope.
    // This must match the scope passed to FacadeWireSchema::register_store() in
    // the geometry-aware operator() overload.
    if (config.contains("use_wire_schema") &&
        config.at("use_wire_schema").as_bool()) {
        m_wcmain.tla_var("wire_schema_name", m_scope);
    }

    m_wcmain.add_app(m_app_type + ":" + m_app_name);
}

void FrameFilter::ensure_initialized()
{
    if (m_initialized.load(std::memory_order_acquire)) return;

    // Serialize all WireCell::Main::initialize() calls: the WCT global factory
    // is not thread-safe, so two concurrent initialize() calls (e.g. from two
    // FrameFilter instances in a PHLEX multi-instance workflow) would corrupt it.
    std::lock_guard<std::mutex> lock(s_wct_init_mutex);
    if (m_initialized.load(std::memory_order_relaxed)) return; // double-check

    m_wcmain.initialize();
    m_initialized.store(true, std::memory_order_release);

    m_source = find_boundary<WireCell::IFrameSource,
                             BoundarySource<WireCell::IFrameSource>>(
                   "FrameBoundarySource", m_src_name);

    m_sink = find_boundary<WireCell::IFrameSink,
                           BoundarySink<WireCell::IFrameSink>>(
                 "FrameBoundarySink", m_snk_name);
}

Frame FrameFilter::operator()(Frame const& input)
{
    ensure_initialized();
    m_source->fill(input.ptr);   // enqueue data for this event
    m_wcmain();                  // run Pgrapher until quiescent
    return Frame{m_sink->drain()};
}

Frame FrameFilter::operator()(WireSchema const& ws, Frame const& input)
{
    // Register geometry in the static map before the first initialize() call.
    // After the first event, this is a no-op because ensure_initialized() returns
    // immediately once m_initialized is true, and the store is already in m_store.
    if (!m_initialized) {
        FacadeWireSchema::register_store(m_scope, ws.store);
    }
    return operator()(input);
}

// ---------------------------------------------------------------------------
// DepoSetToFrame
// ---------------------------------------------------------------------------

DepoSetToFrame::DepoSetToFrame(boost::json::object const& config)
    : Executor(config)
{
    m_src_name = m_scope + "_deposet_source";
    m_snk_name = m_scope + "_frame_sink";
    m_app_name = m_scope + "_pgrapher";

    // Register TLAs and app now; initialize() is deferred to first operator() call.
    m_wcmain.tla_var("source_name", m_src_name);
    m_wcmain.tla_var("sink_name",   m_snk_name);
    m_wcmain.tla_var("app_name",    m_app_name);

    m_wcmain.add_app(m_app_type + ":" + m_app_name);
}

void DepoSetToFrame::ensure_initialized()
{
    if (m_initialized.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(s_wct_init_mutex);
    if (m_initialized.load(std::memory_order_relaxed)) return;

    m_wcmain.initialize();
    m_initialized.store(true, std::memory_order_release);

    m_source = find_boundary<WireCell::IDepoSetSource,
                             BoundarySource<WireCell::IDepoSetSource>>(
                   "DepoSetBoundarySource", m_src_name);

    m_sink = find_boundary<WireCell::IFrameSink,
                           BoundarySink<WireCell::IFrameSink>>(
                 "FrameBoundarySink", m_snk_name);
}

Frame DepoSetToFrame::operator()(DepoSet const& input)
{
    ensure_initialized();
    m_source->fill(input.ptr);   // enqueue data for this event
    m_wcmain();                  // run Pgrapher until quiescent
    return Frame{m_sink->drain()};
}

// ---------------------------------------------------------------------------
// DepoSetSourceFile
// ---------------------------------------------------------------------------

DepoSetSourceFile::DepoSetSourceFile(boost::json::object const& config)
    : Executor(config)
{
    m_snk_name = m_scope + "_deposet_sink";
    m_app_name = m_scope + "_pgrapher";

    m_wcmain.tla_var("sink_name", m_snk_name);
    m_wcmain.tla_var("app_name",  m_app_name);

    m_wcmain.add_app(m_app_type + ":" + m_app_name);
}

void DepoSetSourceFile::ensure_initialized()
{
    if (m_initialized.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(s_wct_init_mutex);
    if (m_initialized.load(std::memory_order_relaxed)) return;

    m_wcmain.initialize();
    m_initialized.store(true, std::memory_order_release);

    m_sink = find_boundary<WireCell::IDepoSetSink,
                           BoundarySink<WireCell::IDepoSetSink>>(
                 "DepoSetBoundarySink", m_snk_name);
}

DepoSet DepoSetSourceFile::operator()()
{
    if (!m_graph_ran.load(std::memory_order_acquire)) {
        ensure_initialized();
        m_wcmain();   // run graph to completion; all depo sets queue in m_sink
        m_graph_ran.store(true, std::memory_order_release);
    }
    return DepoSet{m_sink->drain()};
}

// ---------------------------------------------------------------------------
// DepoSetSinkFile
// ---------------------------------------------------------------------------

DepoSetSinkFile::DepoSetSinkFile(boost::json::object const& config)
    : Executor(config)
{
    m_src_name = m_scope + "_deposet_source";
    m_app_name = m_scope + "_pgrapher";

    m_wcmain.tla_var("source_name", m_src_name);
    m_wcmain.tla_var("app_name",    m_app_name);

    m_wcmain.add_app(m_app_type + ":" + m_app_name);
}

DepoSetSinkFile::~DepoSetSinkFile()
{
    // NOTE: WireCell::Main::~Main() calls finalize() automatically, which
    // invokes ITerminal::finalize() on all terminal components (e.g. DepoFileSink).
    // Do NOT call m_wcmain.finalize() here — that would cause a double-finalize
    // and an empty-chain assertion in boost::iostreams::chain::pop().
}

void DepoSetSinkFile::ensure_initialized()
{
    if (m_initialized.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(s_wct_init_mutex);
    if (m_initialized.load(std::memory_order_relaxed)) return;

    m_wcmain.initialize();
    m_initialized.store(true, std::memory_order_release);

    m_source = find_boundary<WireCell::IDepoSetSource,
                             BoundarySource<WireCell::IDepoSetSource>>(
                   "DepoSetBoundarySource", m_src_name);
}

void DepoSetSinkFile::operator()(DepoSet const& input)
{
    ensure_initialized();
    m_source->fill(input.ptr);
    m_wcmain();
}

// ---------------------------------------------------------------------------
// DepoSetFilter
// ---------------------------------------------------------------------------

DepoSetFilter::DepoSetFilter(boost::json::object const& config)
    : Executor(config)
{
    m_src_name = m_scope + "_deposet_source";
    m_snk_name = m_scope + "_deposet_sink";
    m_app_name = m_scope + "_pgrapher";

    m_wcmain.tla_var("source_name", m_src_name);
    m_wcmain.tla_var("sink_name",   m_snk_name);
    m_wcmain.tla_var("app_name",    m_app_name);

    m_wcmain.add_app(m_app_type + ":" + m_app_name);
}

void DepoSetFilter::ensure_initialized()
{
    if (m_initialized.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(s_wct_init_mutex);
    if (m_initialized.load(std::memory_order_relaxed)) return;

    m_wcmain.initialize();
    m_initialized.store(true, std::memory_order_release);

    m_source = find_boundary<WireCell::IDepoSetSource,
                             BoundarySource<WireCell::IDepoSetSource>>(
                   "DepoSetBoundarySource", m_src_name);

    m_sink = find_boundary<WireCell::IDepoSetSink,
                           BoundarySink<WireCell::IDepoSetSink>>(
                 "DepoSetBoundarySink", m_snk_name);
}

DepoSet DepoSetFilter::operator()(DepoSet const& input)
{
    ensure_initialized();
    m_source->fill(input.ptr);
    m_wcmain();
    return DepoSet{m_sink->drain()};
}

// ---------------------------------------------------------------------------
// FrameSourceFile
// ---------------------------------------------------------------------------

FrameSourceFile::FrameSourceFile(boost::json::object const& config)
    : Executor(config)
{
    m_snk_name = m_scope + "_frame_sink";
    m_app_name = m_scope + "_pgrapher";

    m_wcmain.tla_var("sink_name", m_snk_name);
    m_wcmain.tla_var("app_name",  m_app_name);

    m_wcmain.add_app(m_app_type + ":" + m_app_name);
}

void FrameSourceFile::ensure_initialized()
{
    if (m_initialized.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(s_wct_init_mutex);
    if (m_initialized.load(std::memory_order_relaxed)) return;

    m_wcmain.initialize();
    m_initialized.store(true, std::memory_order_release);

    m_sink = find_boundary<WireCell::IFrameSink,
                           BoundarySink<WireCell::IFrameSink>>(
                 "FrameBoundarySink", m_snk_name);
}

Frame FrameSourceFile::operator()()
{
    if (!m_graph_ran.load(std::memory_order_acquire)) {
        ensure_initialized();
        m_wcmain();   // run graph to completion; all frames queue in m_sink
        m_graph_ran.store(true, std::memory_order_release);
    }
    return Frame{m_sink->drain()};
}

// ---------------------------------------------------------------------------
// FrameSinkFile
// ---------------------------------------------------------------------------

FrameSinkFile::FrameSinkFile(boost::json::object const& config)
    : Executor(config)
{
    m_src_name = m_scope + "_frame_source";
    m_app_name = m_scope + "_pgrapher";

    m_wcmain.tla_var("source_name", m_src_name);
    m_wcmain.tla_var("app_name",    m_app_name);

    m_wcmain.add_app(m_app_type + ":" + m_app_name);
}

FrameSinkFile::~FrameSinkFile()
{
    // NOTE: WireCell::Main::~Main() calls finalize() automatically, which
    // invokes ITerminal::finalize() on all terminal components (e.g. FrameFileSink).
    // Do NOT call m_wcmain.finalize() here — that would cause a double-finalize
    // and an empty-chain assertion in boost::iostreams::chain::pop().
}

void FrameSinkFile::ensure_initialized()
{
    if (m_initialized.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(s_wct_init_mutex);
    if (m_initialized.load(std::memory_order_relaxed)) return;

    m_wcmain.initialize();
    m_initialized.store(true, std::memory_order_release);

    m_source = find_boundary<WireCell::IFrameSource,
                             BoundarySource<WireCell::IFrameSource>>(
                   "FrameBoundarySource", m_src_name);
}

void FrameSinkFile::operator()(Frame const& input)
{
    ensure_initialized();
    m_source->fill(input.ptr);
    m_wcmain();
}

} // namespace wcphlex
