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
// Implementation of Executor base and all concrete subclasses.
//
// Deferred initialization model (mirrors larwirecell WCLS):
//   - Executor() does common config parsing: add_config, add_plugin, tla_var,
//     add_app().  Subclass constructors add boundary-node name TLAs.
//   - The first operator() call triggers ensure_initialized(), which calls
//     m_wcmain.initialize(), then virtual initialize_ports() so the subclass
//     can locate its BoundarySource/BoundarySink instances in the WCT factory.
//   - Each operator() call: fill source(s) → run_graph() → drain sink(s).
//   - BoundarySource's queue design allows re-driving the same Pgraph graph
//     across successive events without re-initialization.
//
// Geometry-aware path (FrameFilter):
//   - operator()(WireSchema const&, Frame const&) calls
//     FacadeWireSchema::register_store(m_scope, ws.store) before the first
//     ensure_initialized().  This pre-populates the static map that
//     FacadeWireSchema::configure() reads during initialize().

#include "wire_cell_phlex/Executor.hpp"
#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"

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

Executor::Executor(ExecutorConfig const& config)
{
    // Required: Jsonnet config file path.  (Absent -> empty default here.)
    const std::string wct_config = config.wct_config;
    if (wct_config.empty()) {
        throw std::runtime_error("Executor: required config field 'wct_config' is missing or empty");
    }
    m_wcmain.add_config(wct_config);

    // WCT app type (stored; subclass builds "type:name" for add_app).
    m_app_type = config.wct_app;

    // Always register wire_cell_phlex as a WCT plugin so WIRECELL_FACTORY
    // registrations (BoundarySource, BoundarySink) are findable by the WCT
    // plugin-based factory lookup (NamedFactory falls back to PluginManager
    // when a classname is not in m_lookup, searching all loaded plugins for
    // make_<ClassName>_factory symbols).
    m_wcmain.add_plugin("wire_cell_phlex");

    // Additional WCT plugins (e.g. "WireCellPgraph").
    for (auto const& p : config.wct_plugins.value) {
        m_wcmain.add_plugin(p);
    }

    // User-supplied Jsonnet TLAs.
    for (auto const& [k, v] : config.wct_tla.value) {
        m_wcmain.tla_var(k, v);
    }

    // Scope prefix for WCT component instance names.  PHLEX injects
    // "module_label" into every module config; use it when non-empty so
    // that two executor instances loaded under different PHLEX module keys
    // create uniquely-named WCT components in the global factory.  Empty
    // keeps the default m_scope ("wcphlex").
    const std::string label = config.module_label;
    if (!label.empty()) {
        m_scope = label;
    }

    // WCT log sink ("stdout", "stderr", or a file path) and level.
    m_log_sink = config.wct_log_sink;
    m_log_level = config.wct_log_level;

    // Compute app instance name and register it.  All subclasses use the same
    // pattern: m_scope + "_pgrapher".  Subclass constructors only need to add
    // their boundary-node TLAs (source_name, sink_name, etc.).
    m_app_name = m_scope + "_pgrapher";
    m_wcmain.tla_var("app_name", m_app_name);
    m_wcmain.add_app(m_app_type + ":" + m_app_name);
}

void Executor::setup_debug_logging()
{
    if (!m_log_sink.empty()) {
        m_wcmain.add_logsink(m_log_sink);
    }
    if (!m_log_level.empty()) {
        m_wcmain.set_loglevel("", m_log_level);
    }
}

void Executor::ensure_initialized()
{
    if (m_initialized.load(std::memory_order_acquire)) return;

    // Serialize all WireCell::Main::initialize() calls: the WCT global factory
    // is not thread-safe, so two concurrent initialize() calls (e.g. from two
    // Executor instances in a PHLEX multi-instance workflow) would corrupt it.
    std::lock_guard<std::mutex> lock(s_wct_init_mutex);
    if (m_initialized.load(std::memory_order_relaxed)) return; // double-check

    setup_debug_logging();
    m_wcmain.initialize();
    initialize_ports();   // virtual: each subclass finds its boundary nodes here

    // Store AFTER initialize_ports() so that any thread seeing m_initialized==true
    // on the fast path is guaranteed to also see fully-assigned boundary pointers.
    m_initialized.store(true, std::memory_order_release);
}

void Executor::initialize_ports()
{
    // Default: no-op.  Subclasses that use BoundarySource/BoundarySink override this.
}

void Executor::run_graph()
{
    m_wcmain();
}

} // namespace wcphlex
