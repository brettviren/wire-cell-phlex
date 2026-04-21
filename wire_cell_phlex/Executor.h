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

} // namespace wcphlex
