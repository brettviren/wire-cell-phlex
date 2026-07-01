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
// Design: WireCell::Main is initialized ONCE on the FIRST operator() call
// (deferred initialization).  Each operator() call fills the boundary source(s),
// runs the WCT graph with run_graph(), then drains the boundary sink(s).  The
// BoundarySource queue-based design allows the same Pgraph graph to be re-driven
// event after event:
//
//   for each PHLEX event:
//       source->fill(input_ptr)    // enqueue data for this event
//       run_graph()                // m_wcmain() — run graph until quiescent
//       result = sink->drain()     // dequeue output
//
// This mirrors the larwirecell WCLS pattern: visit(event) + m_wcmain() + visit().
//
// Deferred initialization rationale:
//   When geometry arrives as a PHLEX job-layer product (wcphlex::WireSchema),
//   it is not available at Executor construction time.  The two-argument overload
//   operator()(WireSchema const&, Frame const&) calls
//   FacadeWireSchema::register_store(m_scope, ws.store) before the first
//   ensure_initialized() call.  This pre-populates the static registry that
//   FacadeWireSchema::configure() reads during initialize().
//
// Lifecycle (see docs/executors.md for the full guide):
//   1. Constructor: Executor() parses config, computes m_scope and m_app_name,
//      registers TLAs and add_app().  Subclass constructors add boundary-node
//      name TLAs.
//   2. First operator() call: ensure_initialized() acquires the global WCT init
//      mutex, calls m_wcmain.initialize(), then calls virtual initialize_ports()
//      so the subclass can find its BoundarySource/BoundarySink instances.
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
#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"
#include "wire_cell_phlex/FacadeWireSchema.hpp"

#include <WireCellApps/Main.h>
#include <WireCellIface/IFrameSource.h>
#include <WireCellIface/IFrameSink.h>
#include <WireCellIface/IDepoSetSource.h>
#include <WireCellIface/IDepoSetSink.h>

#include <boost/json.hpp>

#include <atomic>
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
// Subclass constructors add boundary-node TLAs (source_name, sink_name, etc.).
// initialize() is deferred to the first ensure_initialized() call in operator().
class Executor {
public:
    explicit Executor(boost::json::object const& config);
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

    std::atomic<bool> m_initialized{false};

    // Log sink destination ("stdout", "stderr", or a file path).  Empty = no log setup.
    std::string m_log_sink;

    // Log level string (e.g. "warn", "info", "debug").  Empty = no level set.
    std::string m_log_level;

    // Idempotent initialization guard (DCLP).  Called by every operator().
    // First call: acquires s_wct_init_mutex, calls m_wcmain.initialize(),
    // calls virtual initialize_ports(), then sets m_initialized.
    // Subsequent calls: fast-path return on m_initialized==true.
    void ensure_initialized();

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
    // Called inside ensure_initialized() before m_wcmain.initialize().
    void setup_debug_logging();
};

// ---------------------------------------------------------------------------
// Concrete executor: IFrame → IFrame (signal processing or pass-through).
//
// WCT boundary node instance names are derived from m_scope at construction:
//   source_name = m_scope + "_frame_source"   (FrameBoundarySource)
//   sink_name   = m_scope + "_frame_sink"     (FrameBoundarySink)
//   app_name    = m_scope + "_pgrapher"       (Pgrapher)
//
// Deferred initialization: the WCT graph is not initialized until the first
// operator() call.  This allows geometry (WireSchema) to be registered in
// FacadeWireSchema's static map before initialize() triggers configure().
// ---------------------------------------------------------------------------
class FrameFilter : public Executor {
public:
    explicit FrameFilter(boost::json::object const& config);

    // Process one Frame through the persistent WCT sub-graph.
    // WCT graph is initialized on the first call.
    Frame operator()(Frame const& input);

    // Geometry-aware overload: registers the WireSchema store under m_scope
    // in FacadeWireSchema's static map before initializing WCT on the first call.
    Frame operator()(WireSchema const& ws, Frame const& input);

private:
    void initialize_ports() override;

    std::string m_src_name;
    std::string m_snk_name;

    // Raw pointers into factory-owned objects.  Valid for the lifetime of
    // m_wcmain (i.e. for the lifetime of this FrameFilter instance).
    BoundarySource<WireCell::IFrameSource>* m_source{nullptr};
    BoundarySink<WireCell::IFrameSink>*     m_sink{nullptr};
};

// ---------------------------------------------------------------------------
// Concrete executor: IDepoSet → IFrame (drift + electronics simulation).
//
// WCT boundary node instance names are derived from m_scope at construction:
//   source_name = m_scope + "_deposet_source" (DepoSetBoundarySource)
//   sink_name   = m_scope + "_frame_sink"     (FrameBoundarySink)
//   app_name    = m_scope + "_pgrapher"       (Pgrapher)
// ---------------------------------------------------------------------------
class DepoSetToFrame : public Executor {
public:
    explicit DepoSetToFrame(boost::json::object const& config);

    // Process one DepoSet through the persistent WCT sub-graph.
    // WCT graph is initialized on the first call.
    Frame operator()(DepoSet const& input);

private:
    void initialize_ports() override;

    std::string m_src_name;
    std::string m_snk_name;

    BoundarySource<WireCell::IDepoSetSource>* m_source{nullptr};
    BoundarySink<WireCell::IFrameSink>*       m_sink{nullptr};
};

// ---------------------------------------------------------------------------
// Concrete executor: file → IDepoSet (reads a WCT depo file, yields one DepoSet
// per PHLEX event call).
//
// The WCT sub-graph (e.g. deposet-file-source.jsonnet) includes a real WCT
// source component (e.g. DepoFileSource) and a DepoSetBoundarySink.  On the
// first operator()() call the entire WCT graph is run to completion, queuing
// all depo sets in the sink buffer.  Subsequent calls drain one depo set per
// call.  No BoundarySource is needed; the WCT source component drives the
// graph internally.
//
// WCT boundary node instance names derived from m_scope:
//   sink_name = m_scope + "_deposet_sink"   (DepoSetBoundarySink)
//   app_name  = m_scope + "_pgrapher"       (Pgrapher)
//
// The input file path is passed via wct_tla: { inname: "..." } in the
// module config.
// ---------------------------------------------------------------------------
class DepoSetSourceFile : public Executor {
public:
    explicit DepoSetSourceFile(boost::json::object const& config);

    // On the first call: initialize WCT, run the entire graph, fill the queue.
    // On every call: drain and return one DepoSet from the queue.
    DepoSet operator()();

private:
    void initialize_ports() override;

    std::string m_snk_name;

    std::atomic<bool>                         m_graph_ran{false};
    BoundarySink<WireCell::IDepoSetSink>*     m_sink{nullptr};
};

// ---------------------------------------------------------------------------
// Concrete executor: IDepoSet → file (writes each received DepoSet to a WCT
// depo file via DepoFileSink).
//
// The WCT sub-graph (e.g. deposet-file-sink.jsonnet) includes a
// DepoSetBoundarySource and a real WCT sink component (e.g. DepoFileSink).
// Each operator()(DepoSet) call fills the boundary source with the given depo
// set and runs the graph once.  The destructor calls m_wcmain.finalize() to
// flush the WCT sink (ITerminal).
//
// WCT boundary node instance names derived from m_scope:
//   source_name = m_scope + "_deposet_source"  (DepoSetBoundarySource)
//   app_name    = m_scope + "_pgrapher"        (Pgrapher)
//
// The output file path is passed via wct_tla: { outname: "..." } in the
// module config.
// ---------------------------------------------------------------------------
class DepoSetSinkFile : public Executor {
public:
    explicit DepoSetSinkFile(boost::json::object const& config);
    ~DepoSetSinkFile() override;

    // Deliver one DepoSet to the WCT sub-graph.  Graph is initialized on first
    // call.
    void operator()(DepoSet const& input);

private:
    void initialize_ports() override;

    std::string m_src_name;

    BoundarySource<WireCell::IDepoSetSource>* m_source{nullptr};
};

// ---------------------------------------------------------------------------
// Concrete executor: IDepoSet → IDepoSet (drift or pass-through).
//
// WCT boundary node instance names are derived from m_scope at construction:
//   source_name = m_scope + "_deposet_source"  (DepoSetBoundarySource)
//   sink_name   = m_scope + "_deposet_sink"    (DepoSetBoundarySink)
//   app_name    = m_scope + "_pgrapher"        (Pgrapher)
// ---------------------------------------------------------------------------
class DepoSetFilter : public Executor {
public:
    explicit DepoSetFilter(boost::json::object const& config);

    // Process one DepoSet through the persistent WCT sub-graph.
    // WCT graph is initialized on the first call.
    DepoSet operator()(DepoSet const& input);

private:
    void initialize_ports() override;

    std::string m_src_name;
    std::string m_snk_name;

    BoundarySource<WireCell::IDepoSetSource>* m_source{nullptr};
    BoundarySink<WireCell::IDepoSetSink>*     m_sink{nullptr};
};

// ---------------------------------------------------------------------------
// Concrete executor: file → IFrame (reads a WCT frame file, yields one Frame
// per PHLEX event call).
//
// Mirrors DepoSetSourceFile but for IFrame.  The WCT sub-graph (e.g.
// frame-file-source.jsonnet) contains a real WCT source component (e.g.
// FrameFileSource) and a FrameBoundarySink.  On the first operator()() call
// the entire WCT graph is run to completion, queuing all frames in the sink
// buffer.  Subsequent calls drain one Frame per call.  No BoundarySource is
// needed; the WCT source component drives the graph internally.
//
// WCT boundary node instance names derived from m_scope:
//   sink_name = m_scope + "_frame_sink"   (FrameBoundarySink)
//   app_name  = m_scope + "_pgrapher"     (Pgrapher)
//
// The input file path is passed via wct_tla: { inname: "..." } in the
// module config.
// ---------------------------------------------------------------------------
class FrameSourceFile : public Executor {
public:
    explicit FrameSourceFile(boost::json::object const& config);

    // On the first call: initialize WCT, run the entire graph, fill the queue.
    // On every call: drain and return one Frame from the queue.
    Frame operator()();

private:
    void initialize_ports() override;

    std::string m_snk_name;

    std::atomic<bool>                      m_graph_ran{false};
    BoundarySink<WireCell::IFrameSink>*    m_sink{nullptr};
};

// ---------------------------------------------------------------------------
// Concrete executor: IFrame → file (writes each received Frame to a WCT
// frame file via FrameFileSink).
//
// Mirrors DepoSetSinkFile but for IFrame instead of IDepoSet.  The WCT
// sub-graph (e.g. frame-file-sink.jsonnet) includes a FrameBoundarySource
// and a real WCT sink component (e.g. FrameFileSink).  Each
// operator()(Frame) call fills the boundary source and runs the graph once.
// WireCell::Main::~Main() calls finalize() automatically, which flushes the
// FrameFileSink output file.
//
// WCT boundary node instance names derived from m_scope:
//   source_name = m_scope + "_frame_source"  (FrameBoundarySource)
//   app_name    = m_scope + "_pgrapher"      (Pgrapher)
//
// The output file path is passed via wct_tla: { outname: "..." } in the
// module config.
// ---------------------------------------------------------------------------
class FrameSinkFile : public Executor {
public:
    explicit FrameSinkFile(boost::json::object const& config);
    ~FrameSinkFile() override;  // empty body — Main::~Main() calls finalize()

    // Deliver one Frame to the WCT sub-graph.  Graph is initialized on first call.
    void operator()(Frame const& input);

private:
    void initialize_ports() override;

    std::string m_src_name;

    BoundarySource<WireCell::IFrameSource>* m_source{nullptr};
};

// ---------------------------------------------------------------------------
// Concrete executor: N×IFrame → file (receives N Frames, merges via WCT
// FrameFanin, writes the merged Frame via FrameFileSink).
//
// Fixed multiplicity 4.  The WCT sub-graph (e.g. frame-fanin-file-sink.jsonnet)
// has 4 FrameBoundarySource nodes feeding FrameFanin, which feeds FrameFileSink.
// Each operator()(f0, f1, f2, f3) call fills all 4 sources and runs the graph
// once.  WireCell::Main::~Main() calls finalize() to flush the file.
//
// WCT boundary node instance names derived from m_scope:
//   source_name_N = m_scope + "_frame_source_N"  for N in {0,1,2,3}
//   app_name      = m_scope + "_pgrapher"
//
// The output file path is passed via wct_tla: { outname: "..." }.
// ---------------------------------------------------------------------------
class FrameFaninSinkFile : public Executor {
public:
    explicit FrameFaninSinkFile(boost::json::object const& config);
    ~FrameFaninSinkFile() override;  // empty body — Main::~Main() calls finalize()

    // Receive 4 Frames, fill 4 boundary sources, run WCT (FrameFanin → FrameFileSink).
    void operator()(Frame const& f0, Frame const& f1,
                    Frame const& f2, Frame const& f3);

private:
    void initialize_ports() override;

    static constexpr int k_multiplicity = 4;

    std::array<std::string, k_multiplicity>                             m_src_names;
    std::array<BoundarySource<WireCell::IFrameSource>*, k_multiplicity> m_sources{};
};

} // namespace wcphlex
