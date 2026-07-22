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

// wire_cell_phlex/FunctionExecutor.hpp  (Idea-2 spike, ddm-7dk.3)
//
// A templated mid-level Executor that factors node SHAPE (1-in/1-out function)
// from the PORT TYPES, analogous to WCT's IFunctionNode<In,Out>.  Instead of a
// hand-written subclass per data-type combination, one template + a port_traits
// row + a `using` alias suffices:
//
//     using FrameFilter = FunctionExecutor<WireCell::IFrame, WireCell::IFrame>;
//
// It addresses the WCT graph through the *generic* mid-level interfaces
// ISourceNode<IType>/ISinkNode<IType> rather than the concrete IFrameSource/…,
// because Pgraph wires edges by data type (port signatures come from
// input_types()/output_types() = typeid(IType)), not by the node interface.

#include "wire_cell_phlex/Executor.hpp"
#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"
#include "wire_cell_phlex/find_boundary.hpp"

#include <WireCellIface/ISourceNode.h>
#include <WireCellIface/ISinkNode.h>

#include <memory>
#include <string>

namespace wcphlex {

// port_traits<IType>: the WCT plumbing for one data type.  The interfaces are
// *derived* generically (no per-type interface naming); only the boundary
// factory names and the instance-name stem are per-type strings (the residual
// obligation of WCT's string-keyed factory — a small, loopable set).
template <class IType>
struct port_traits;   // must be specialized per supported type

template <>
struct port_traits<WireCell::IFrame> {
    using source_iface = WireCell::ISourceNode<WireCell::IFrame>;
    using sink_iface = WireCell::ISinkNode<WireCell::IFrame>;
    static constexpr const char* src_class = "GenericFrameBoundarySource";
    static constexpr const char* snk_class = "GenericFrameBoundarySink";
    static constexpr const char* stem = "frame";
};

// A 1-in / 1-out function node backed by a WCT sub-graph.
template <class In, class Out>
class FunctionExecutor : public Executor {
    using ST = port_traits<In>;
    using DT = port_traits<Out>;

public:
    explicit FunctionExecutor(ExecutorConfig const& config)
        : Executor(config)
    {
        m_src_name = m_scope + "_" + ST::stem + "_source";
        m_snk_name = m_scope + "_" + DT::stem + "_sink";
        m_wcmain.tla_var("source_name", m_src_name);
        m_wcmain.tla_var("sink_name", m_snk_name);
    }

    Data<Out> operator()(Data<In> const& input)
    {
        ensure_initialized();
        m_source->fill(input.ptr);
        run_graph();
        return Data<Out>{m_sink->drain()};
    }

private:
    void initialize_ports() override
    {
        m_source = find_boundary<typename ST::source_iface,
                                 BoundarySource<typename ST::source_iface>>(
            ST::src_class, m_src_name);
        m_sink = find_boundary<typename DT::sink_iface,
                               BoundarySink<typename DT::sink_iface>>(
            DT::snk_class, m_snk_name);
    }

    std::string m_src_name;
    std::string m_snk_name;
    std::shared_ptr<BoundarySource<typename ST::source_iface>> m_source;
    std::shared_ptr<BoundarySink<typename DT::sink_iface>> m_sink;
};

} // namespace wcphlex
