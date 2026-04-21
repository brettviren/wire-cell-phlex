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
// runs the WCT graph with m_wcmain(), then drains the boundary sink(s).  The
// BoundarySource queue-based design allows the same Pgraph graph to be re-driven
// event after event:
//
//   for each PHLEX event:
//       source->fill(input_ptr)   // enqueue data for this event
//       m_wcmain()                // run graph until quiescent
//       result = sink->drain()    // dequeue output
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
// Config interface: boost::json::object (not phlex::configuration) so this
// header compiles under GCC 12, which lacks std::forward_like used in
// phlex/core/product_query.hpp.  PHLEX MODULE files (Step 5+) build a
// boost::json::object from phlex::configuration before constructing an Executor.
//
// Expected JSON keys:
//   wct_config  (string, required): path to a Jsonnet config file.
//               The file must be a Jsonnet function accepting at minimum the
//               TLA parameters: source_name, sink_name, app_name.
//   wct_plugins (array of strings, optional): additional WCT plugin libraries
//               to load (e.g. ["WireCellPgraph"]).
//   wct_app     (string, optional, default "Pgrapher"): WCT IApplication type.
//   wct_tla     (object, optional): string→string map of extra Jsonnet TLAs.

#include "wire_cell_phlex/Data.h"
#include "wire_cell_phlex/BoundarySource.h"
#include "wire_cell_phlex/BoundarySink.h"
#include "wire_cell_phlex/FacadeWireSchema.h"

#include <WireCellApps/Main.h>
#include <WireCellIface/IFrameSource.h>
#include <WireCellIface/IFrameSink.h>
#include <WireCellIface/IDepoSetSource.h>
#include <WireCellIface/IDepoSetSink.h>

#include <boost/json.hpp>

#include <atomic>
#include <string>

namespace wcphlex {

// Base class: common config parsing and WCT plugin/TLA setup.
//
// Executor() parses the boost::json::object config and calls:
//   m_wcmain.add_config(...)
//   m_wcmain.add_plugin("wire_cell_phlex")  // always
//   m_wcmain.add_plugin(...) for each entry in wct_plugins
//   m_wcmain.tla_var(k, v)  for each entry in wct_tla
//
// It does NOT call add_app() or initialize() — those are deferred to the first
// ensure_initialized() call in operator().  Subclasses store boundary-node names
// and app name, injected as TLAs, which are passed to ensure_initialized().
class Executor {
public:
    explicit Executor(boost::json::object const& config);
    virtual ~Executor() = default;

    Executor(Executor const&)            = delete;
    Executor& operator=(Executor const&) = delete;

protected:
    WireCell::Main m_wcmain;
    std::string    m_app_type{"Pgrapher"}; // from wct_app; subclass builds "type:name"

    // Scope prefix for all WCT component instance names created by this executor.
    // Populated from the "module_label" key PHLEX injects into every module's
    // config; defaults to "wcphlex" when used outside PHLEX (unit tests).
    // Using the module label guarantees that two executor instances loaded under
    // different PHLEX module keys (e.g. "sigproc_a", "sigproc_b") create WCT
    // components with distinct names in the global WCT factory, preventing
    // cross-instance aliasing.
    std::string    m_scope{"wcphlex"};

    std::atomic<bool> m_initialized{false};
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
    // Initialize WCT graph on the first call (idempotent).
    void ensure_initialized();

    std::string m_src_name;
    std::string m_snk_name;
    std::string m_app_name;

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
    // Initialize WCT graph on the first call (idempotent).
    void ensure_initialized();

    std::string m_src_name;
    std::string m_snk_name;
    std::string m_app_name;

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
    void ensure_initialized();

    std::string m_snk_name;
    std::string m_app_name;

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
    void ensure_initialized();

    std::string m_src_name;
    std::string m_app_name;

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
    // Initialize WCT graph on the first call (idempotent).
    void ensure_initialized();

    std::string m_src_name;
    std::string m_snk_name;
    std::string m_app_name;

    BoundarySource<WireCell::IDepoSetSource>* m_source{nullptr};
    BoundarySink<WireCell::IDepoSetSink>*     m_sink{nullptr};
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
    void ensure_initialized();

    std::string m_src_name;
    std::string m_app_name;

    BoundarySource<WireCell::IFrameSource>* m_source{nullptr};
};

} // namespace wcphlex
