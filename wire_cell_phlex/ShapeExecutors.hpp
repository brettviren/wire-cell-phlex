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

// wire_cell_phlex/ShapeExecutors.hpp  (Idea-2, ddm-7dk.3/.4)
//
// A family of mid-level templated Executors, one per Phlex/WCT node SHAPE, all
// factored onto a single base parameterised by the input and output PORT TYPE
// LISTS.  Analogous to WCT's INode taxonomy (IFunctionNode, IFaninNode, …) but
// for the Phlex-facing side: each shape wraps a WCT sub-graph whose data
// crosses the Phlex boundary through BoundarySource / BoundarySink nodes.
//
//   shape                inputs (boundary sources)   outputs (boundary sinks)
//   -----                -------------------------   -----------------------
//   FunctionExecutor<In,Out>          1 (In)                   1 (Out)
//   SinkExecutor<In>                  1 (In)                   0   (real WCT sink)
//   SourceExecutor<Out>               0   (real WCT source)    1 (Out)
//   FaninExecutor<In,Out,N>           N (In, homogeneous)      1 (Out)
//   FanoutExecutor<In,Out,N>          1 (In)                   N (Out, homogeneous)
//   JoinExecutor<tuple<Ins...>,Out>   N (Ins..., heterogeneous)1 (Out)
//   SplitExecutor<In,tuple<Outs...>>  1 (In)                   N (Outs..., heterog.)
//
// The base does everything shape-independent: it holds the boundary node
// handles, injects their instance names as WCT TLAs (source_name_<i> /
// sink_name_<j>), finds them after initialize(), and provides run(ins...) which
// fills the sources, runs the WCT graph once, and drains the sinks.  A shape is
// then just the ergonomic operator() over that core.

#include "wire_cell_phlex/Executor.hpp"
#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"
#include "wire_cell_phlex/find_boundary.hpp"

#include <WireCellIface/ISourceNode.h>
#include <WireCellIface/ISinkNode.h>
#include <WireCellIface/IFrame.h>
#include <WireCellIface/IDepoSet.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace wcphlex {

// ---------------------------------------------------------------------------
// port_traits<IType>: the WCT plumbing for one data type.  The source/sink
// interfaces are DERIVED generically (ISourceNode<IType>/ISinkNode<IType> — see
// the spike: Pgraph wires by data type, so the concrete IFrameSource/… are not
// needed).  Only the boundary factory class-names are per-type strings — the
// one residual obligation of WCT's string-keyed factory.
// ---------------------------------------------------------------------------
template <class IType>
struct port_traits;

template <>
struct port_traits<WireCell::IFrame> {
    using source_iface = WireCell::ISourceNode<WireCell::IFrame>;
    using sink_iface = WireCell::ISinkNode<WireCell::IFrame>;
    static constexpr const char* src_class = "GenericFrameBoundarySource";
    static constexpr const char* snk_class = "GenericFrameBoundarySink";
};

template <>
struct port_traits<WireCell::IDepoSet> {
    using source_iface = WireCell::ISourceNode<WireCell::IDepoSet>;
    using sink_iface = WireCell::ISinkNode<WireCell::IDepoSet>;
    static constexpr const char* src_class = "GenericDepoSetBoundarySource";
    static constexpr const char* snk_class = "GenericDepoSetBoundarySink";
};

// A storage-free type-list carrier for the port types.  (std::tuple cannot be
// used here: the WCT interface types — IFrame, IDepoSet, … — are abstract, and
// std::tuple<Abstract...> is ill-formed even as a mere type carrier.  The body
// only ever stores std::tuple<shared_ptr<...>> / std::tuple<Data<...>>, which
// are concrete and fine.)
template <class... Ts>
struct type_list {};

namespace detail {
template <class T, std::size_t>
using repeat_elem = T;   // yields T regardless of the index
template <class T, class Seq>
struct repeat_list_impl;
template <class T, std::size_t... I>
struct repeat_list_impl<T, std::index_sequence<I...>> {
    using type = type_list<repeat_elem<T, I>...>;
};
} // namespace detail

// repeat_list<T,N> == type_list<T, T, ... (N times)>
template <class T, std::size_t N>
using repeat_list = typename detail::repeat_list_impl<T, std::make_index_sequence<N>>::type;

// ---------------------------------------------------------------------------
// PortedExecutor<type_list<Ins...>, type_list<Outs...>> — the workhorse base.
// ---------------------------------------------------------------------------
template <class InList, class OutList>
class PortedExecutor;

template <class... Ins, class... Outs>
class PortedExecutor<type_list<Ins...>, type_list<Outs...>> : public Executor {
public:
    explicit PortedExecutor(ExecutorConfig const& config)
        : Executor(config)
    {
        inject_tlas(std::make_index_sequence<n_in>{}, std::make_index_sequence<n_out>{});
    }

protected:
    static constexpr std::size_t n_in = sizeof...(Ins);
    static constexpr std::size_t n_out = sizeof...(Outs);

    // Fill each input source, run the WCT graph once, drain each output sink.
    std::tuple<Data<Outs>...> run(Data<Ins> const&... ins)
    {
        ensure_initialized();
        fill_all(std::forward_as_tuple(ins...), std::make_index_sequence<n_in>{});
        run_graph();
        return drain_all(std::make_index_sequence<n_out>{});
    }

    void initialize_ports() override
    {
        find_sources(std::make_index_sequence<n_in>{});
        find_sinks(std::make_index_sequence<n_out>{});
    }

    // Per-port WCT instance names, scoped so instances of different modules
    // never collide in the global WCT factory.
    std::string src_name(std::size_t i) const { return m_scope + "_source_" + std::to_string(i); }
    std::string snk_name(std::size_t j) const { return m_scope + "_sink_" + std::to_string(j); }

    std::tuple<std::shared_ptr<BoundarySource<typename port_traits<Ins>::source_iface>>...> m_sources;
    std::tuple<std::shared_ptr<BoundarySink<typename port_traits<Outs>::sink_iface>>...> m_sinks;

private:
    template <std::size_t... I, std::size_t... J>
    void inject_tlas(std::index_sequence<I...>, std::index_sequence<J...>)
    {
        (m_wcmain.tla_var("source_name_" + std::to_string(I), src_name(I)), ...);
        (m_wcmain.tla_var("sink_name_" + std::to_string(J), snk_name(J)), ...);
    }

    template <std::size_t... I>
    void find_sources(std::index_sequence<I...>)
    {
        ((std::get<I>(m_sources) =
              find_boundary<typename port_traits<Ins>::source_iface,
                            BoundarySource<typename port_traits<Ins>::source_iface>>(
                  port_traits<Ins>::src_class, src_name(I))),
         ...);
    }

    template <std::size_t... J>
    void find_sinks(std::index_sequence<J...>)
    {
        ((std::get<J>(m_sinks) =
              find_boundary<typename port_traits<Outs>::sink_iface,
                            BoundarySink<typename port_traits<Outs>::sink_iface>>(
                  port_traits<Outs>::snk_class, snk_name(J))),
         ...);
    }

    template <class InRefs, std::size_t... I>
    void fill_all(InRefs&& ins, std::index_sequence<I...>)
    {
        (std::get<I>(m_sources)->fill(std::get<I>(ins).ptr), ...);
    }

    template <std::size_t... J>
    std::tuple<Data<Outs>...> drain_all(std::index_sequence<J...>)
    {
        return std::tuple<Data<Outs>...>{Data<Outs>{std::get<J>(m_sinks)->drain()}...};
    }
};

// ---------------------------------------------------------------------------
// Shapes.
// ---------------------------------------------------------------------------

// 1 -> 1 : a transform.
template <class In, class Out>
class FunctionExecutor : public PortedExecutor<type_list<In>, type_list<Out>> {
public:
    using PortedExecutor<type_list<In>, type_list<Out>>::PortedExecutor;
    Data<Out> operator()(Data<In> const& in) { return std::get<0>(this->run(in)); }
};

// 1 -> 0 : an observer / terminal sink (the WCT graph ends in a real sink).
template <class In>
class SinkExecutor : public PortedExecutor<type_list<In>, type_list<>> {
public:
    using PortedExecutor<type_list<In>, type_list<>>::PortedExecutor;
    void operator()(Data<In> const& in) { this->run(in); }
};

// N -> 1 (heterogeneous inputs) : a join.  The multi-port side is a type_list.
template <class InList, class Out>
class JoinExecutor;
template <class... Ins, class Out>
class JoinExecutor<type_list<Ins...>, Out>
    : public PortedExecutor<type_list<Ins...>, type_list<Out>> {
public:
    using PortedExecutor<type_list<Ins...>, type_list<Out>>::PortedExecutor;
    Data<Out> operator()(Data<Ins> const&... ins) { return std::get<0>(this->run(ins...)); }
};

// 1 -> N (heterogeneous outputs) : a split.  The multi-port side is a type_list.
template <class In, class OutList>
class SplitExecutor;
template <class In, class... Outs>
class SplitExecutor<In, type_list<Outs...>>
    : public PortedExecutor<type_list<In>, type_list<Outs...>> {
public:
    using PortedExecutor<type_list<In>, type_list<Outs...>>::PortedExecutor;
    std::tuple<Data<Outs>...> operator()(Data<In> const& in) { return this->run(in); }
};

// N -> 1 (homogeneous inputs) : a fan-in — a Join whose N inputs share a type.
template <class In, class Out, std::size_t N>
using FaninExecutor = JoinExecutor<repeat_list<In, N>, Out>;

// 1 -> N (homogeneous outputs) : a fan-out — a Split whose N outputs share a type.
// (Returns std::tuple<Data<Out>...>; a std::array<Data<Out>,N> convenience form
//  could be layered on if a caller prefers it.)
template <class In, class Out, std::size_t N>
using FanoutExecutor = SplitExecutor<In, repeat_list<Out, N>>;

// 0 -> 1 : a source (the WCT graph contains a real source, e.g. a file reader).
// Its execution protocol differs: the first call runs the graph to completion
// (queuing every output in the boundary sink); each call drains one.
template <class Out>
class SourceExecutor : public PortedExecutor<type_list<>, type_list<Out>> {
public:
    using PortedExecutor<type_list<>, type_list<Out>>::PortedExecutor;
    Data<Out> operator()()
    {
        if (!m_ran.exchange(true)) {
            this->ensure_initialized();
            this->run_graph();
        }
        return Data<Out>{std::get<0>(this->m_sinks)->drain()};
    }

private:
    std::atomic<bool> m_ran{false};
};

} // namespace wcphlex
