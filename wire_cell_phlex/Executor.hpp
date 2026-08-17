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

// wire_cell_phlex/Executor.h
//
// Executor base class and concrete subclasses for per-event WCT graph execution.
//
// Design: WireCell::Main is initialized ONCE, in the concrete executor's
// constructor (construction-time initialization).  Each operator() call fills
// the boundary source(s), runs the WCT graph with run_graph(), then drains the
// boundary sink(s).  The BoundarySource queue-based design allows the same
// Pgraph graph to be re-driven event after event:
//
//   for each PHLEX event:
//       source->fill(input_ptr)    // enqueue data for this event
//       run_graph()                // m_wcmain() — run graph until quiescent
//       result = sink->drain()     // dequeue output
//
// This mirrors the larwirecell WCLS pattern: visit(event) + m_wcmain() + visit().
//
// Geometry-as-PHLEX-product (FacadeWireSchema) note:
//   A previous design deferred initialization to the first event so geometry
//   arriving as a PHLEX job-layer product could be side-channelled into WCT's
//   configure-time IWireSchema service (FacadeWireSchema) before
//   m_wcmain.initialize() ran.  That path is INCOMPATIBLE with construction-time
//   initialization: AnodePlane::configure() consumes the wire schema during
//   initialize() — now at construction — before any run-time product can arrive.
//   It is therefore currently disabled (FacadeWireSchema is not built).  Restoring
//   it needs either a split WCT Main::initialize() (compile config on the main
//   thread; instantiate/configure once the product arrives) or an upstream
//   main-thread post-construction hook (ddm-4or.4).
//
// Lifecycle (see docs/executors.md for the full guide):
//   1. Base Executor() parses config, computes m_scope and m_app_name, registers
//      TLAs and add_app().
//   2. The concrete (leaf) constructor injects its boundary-node name TLAs and,
//      as its final step, calls initialize_now(): m_wcmain.initialize() then
//      virtual initialize_ports() so the leaf finds its BoundarySource/
//      BoundarySink instances.  This runs on the main thread during PHLEX
//      registration (no mutex needed; construction is serial).
//   3. Each operator() call: fill sources → run_graph() → drain sinks.
//
// Config interface: boost::json::object (not phlex::configuration) so this
// header compiles under GCC 12, which lacks std::forward_like used in
// phlex/core/product_selector.hpp.  PHLEX MODULE files (Step 5+) build a
// boost::json::object from phlex::configuration before constructing an Executor.
//
// Expected JSON keys:
//   wct_config    (string, required): path to a Jsonnet config file.
//                 The file must be a Jsonnet function accepting at minimum the
//                 TLA parameters: source_name, sink_name, app_name.
//   wct_plugins   (array of strings, optional): additional WCT plugin libraries
//                 to load (e.g. ["WireCellPgraph"]).
//   wct_app       (string, optional, default "Pgrapher"): WCT IApplication type.
//   wct_tla       (object, optional): string→string map of extra Jsonnet TLAs.
//   wct_log_sink  (string, optional, default ""): when non-empty, route WCT log
//                 output to this destination before initializing the WCT graph.
//                 Accepted values: "stdout", "stderr", or a file path.
//                 Useful for diagnosing OmnibusSigProc channel-map issues
//                 (the configure() log shows per-face wire counts and channel
//                 ranges used to set m_nwires).
//   wct_log_level (string, optional, default ""): when non-empty, set the WCT
//                 log level.  Accepted values: "warn", "info", "debug", etc.
//                 Only meaningful when wct_log_sink is also set.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/Config.hpp"
#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"

#include <WireCellApps/Main.h>
#include <WireCellIface/IFrameSource.h>
#include <WireCellIface/IFrameSink.h>
#include <WireCellIface/IDepoSetSource.h>
#include <WireCellIface/IDepoSetSink.h>

#include <boost/json.hpp>

#include <string>

namespace wcphlex {

// Base class: common config parsing, deferred initialization, and graph execution.
//
// Executor() parses the boost::json::object config and calls:
//   m_wcmain.add_config(...)
//   m_wcmain.add_plugin("wire_cell_phlex")  // always
//   m_wcmain.add_plugin(...) for each entry in wct_plugins
//   m_wcmain.tla_var(k, v)  for each entry in wct_tla
//   m_wcmain.tla_var("app_name", m_app_name)
//   m_wcmain.add_app(m_app_type + ":" + m_app_name)
//
// Subclass constructors add boundary-node TLAs (source_name, sink_name, etc.)
// and then call initialize_now() as their final step.
class Executor {
public:
    // Constructed from a parsed ExecutorConfig.  Concrete subclasses parse
    // their own *Config (which carries an `executor` field) from the
    // boost::json::object PHLEX supplies and pass that field here.
    explicit Executor(ExecutorConfig const& config);
    virtual ~Executor() = default;

    Executor(Executor const&)            = delete;
    Executor& operator=(Executor const&) = delete;

protected:
    WireCell::Main m_wcmain;
    std::string    m_app_type{"Pgrapher"}; // from wct_app; used to build add_app() name

    // Scope prefix for all WCT component instance names created by this executor.
    // Populated from the "module_label" key PHLEX injects into every module's
    // config; defaults to "wcphlex" when used outside PHLEX (unit tests).
    // Using the module label guarantees that two executor instances loaded under
    // different PHLEX module keys (e.g. "sigproc_a", "sigproc_b") create WCT
    // components with distinct names in the global WCT factory, preventing
    // cross-instance aliasing.
    std::string    m_scope{"wcphlex"};

    // App instance name: always m_scope + "_pgrapher".  Set in Executor()
    // before add_app() is called; available to subclasses for reference.
    std::string    m_app_name;

    // Log sink destination ("stdout", "stderr", or a file path).  Empty = no log setup.
    std::string m_log_sink;

    // Log level string (e.g. "warn", "info", "debug").  Empty = no level set.
    std::string m_log_level;

    // Build and configure the WCT graph, then bind boundary ports.  Called once,
    // as the final step of each concrete executor's constructor (main thread,
    // during PHLEX registration).  Runs setup_debug_logging(), then
    // m_wcmain.initialize(), then virtual initialize_ports().  Not thread-safe by
    // itself: correctness relies on serial, single-threaded module construction.
    void initialize_now();

    // Override in concrete subclasses to find BoundarySource/BoundarySink
    // instances in the WCT factory after m_wcmain.initialize() completes.
    // Default implementation is a no-op (safe for subclasses with no boundary nodes).
    virtual void initialize_ports();

    // Execute the WCT graph: calls m_wcmain().  Called by every operator()
    // after filling boundary sources.
    void run_graph();

private:
    // If m_log_sink is non-empty, calls add_logsink(m_log_sink) on m_wcmain.
    // If m_log_level is non-empty, calls set_loglevel("", m_log_level) on m_wcmain.
    // Called inside initialize_now() before m_wcmain.initialize().
    void setup_debug_logging();
};

} // namespace wcphlex
