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
//   FaninExecutor<In,Out>             N (In, DYNAMIC mult.)    1 (Out)
//   FanoutExecutor<In,Out>            1 (In)                   N (Out, DYNAMIC mult.)
//   JoinExecutor<type_list<Ins...>,Out> N (Ins..., heterog.)   1 (Out)
//   SplitExecutor<In,type_list<Outs...>> 1 (In)                N (Outs..., heterog.)
//
// Fans differ from Join/Split: WCT fans size their port count from
// configuration at run time (e.g. FrameFanin overriding input_types()), so a
// fan's multiplicity is a runtime value, not a template parameter.  The Phlex
// side therefore deals in a std::vector<Data<IType>> (homogeneous), bridged to
// the WCT std::vector<IType::pointer> by a plain iteration.
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
#include <WireCellIface/ITrackSegmentSet.h>
#include <WireCellIface/IBlobSet.h>
#include <WireCellIface/ICluster.h>

#include <boost/json.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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
    static constexpr const char* src_class = "FrameBoundarySource";
    static constexpr const char* snk_class = "FrameBoundarySink";
    static constexpr const char* stem = "frame";   // <type>: IFrame -> frame
};

template <>
struct port_traits<WireCell::IDepoSet> {
    using source_iface = WireCell::ISourceNode<WireCell::IDepoSet>;
    using sink_iface = WireCell::ISinkNode<WireCell::IDepoSet>;
    static constexpr const char* src_class = "DepoSetBoundarySource";
    static constexpr const char* snk_class = "DepoSetBoundarySink";
    static constexpr const char* stem = "deposet"; // <type>: IDepoSet -> deposet
};

template <>
struct port_traits<WireCell::ITrackSegmentSet> {
    using source_iface = WireCell::ISourceNode<WireCell::ITrackSegmentSet>;
    using sink_iface = WireCell::ISinkNode<WireCell::ITrackSegmentSet>;
    static constexpr const char* src_class = "TrackSegmentSetBoundarySource";
    static constexpr const char* snk_class = "TrackSegmentSetBoundarySink";
    static constexpr const char* stem = "tracksegmentset"; // ITrackSegmentSet -> tracksegmentset
};

template <>
struct port_traits<WireCell::IBlobSet> {
    using source_iface = WireCell::ISourceNode<WireCell::IBlobSet>;
    using sink_iface = WireCell::ISinkNode<WireCell::IBlobSet>;
    static constexpr const char* src_class = "BlobSetBoundarySource";
    static constexpr const char* snk_class = "BlobSetBoundarySink";
    static constexpr const char* stem = "blobset"; // <type>: IBlobSet -> blobset
};

template <>
struct port_traits<WireCell::ICluster> {
    using source_iface = WireCell::ISourceNode<WireCell::ICluster>;
    using sink_iface = WireCell::ISinkNode<WireCell::ICluster>;
    static constexpr const char* src_class = "ClusterBoundarySource";
    static constexpr const char* snk_class = "ClusterBoundarySink";
    static constexpr const char* stem = "cluster"; // <type>: ICluster -> cluster
};

// The naming-convention <type> token for a WCT IData type (the "I" removed,
// lower-cased): IFrame -> "frame", IDepoSet -> "deposet".
template <class IType>
std::string type_stem()
{
    return port_traits<IType>::stem;
}

// A storage-free type-list carrier for the port types.  (std::tuple cannot be
// used here: the WCT interface types — IFrame, IDepoSet, … — are abstract, and
// std::tuple<Abstract...> is ill-formed even as a mere type carrier.  The body
// only ever stores std::tuple<shared_ptr<...>> / std::tuple<Data<...>>, which
// are concrete and fine.)
template <class... Ts>
struct type_list {
    static constexpr std::size_t size = sizeof...(Ts);
};

// Per-port WCT instance names, scoped so instances of different modules never
// collide in the global WCT factory.  Shared by every shape.
inline std::string port_source_name(std::string const& scope, std::size_t i)
{
    return scope + "_source_" + std::to_string(i);
}
inline std::string port_sink_name(std::string const& scope, std::size_t j)
{
    return scope + "_sink_" + std::to_string(j);
}

// Inject the boundary-node handles into the WCT sub-graph as two code-valued
// TLAs, "sources" and "sinks".  Each is an array of WCT "inode" objects
// { type: <WCT class name>, name: <instance name> } — ready for a Jsonnet
// config to drop in as component definitions and reference in Pgraph edges.
// Both arrays are always injected (either may be empty), so every shape's
// Jsonnet function signature is the uniform (sources=[], sinks=[], app_name).
// Passing the type this way means the Jsonnet never hard-codes a boundary class
// name, and the port count is not baked into the function signature.
inline void inject_boundary_tlas(
    WireCell::Main& wcmain,
    std::vector<std::pair<std::string, std::string>> const& sources,
    std::vector<std::pair<std::string, std::string>> const& sinks)
{
    auto to_code = [](std::vector<std::pair<std::string, std::string>> const& ports) {
        boost::json::array arr;
        for (auto const& [type, name] : ports) {
            arr.push_back(boost::json::object{{"type", type}, {"name", name}});
        }
        return boost::json::serialize(boost::json::value(std::move(arr)));
    };
    wcmain.tla_code("sources", to_code(sources));
    wcmain.tla_code("sinks", to_code(sinks));
}

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
        // Final step: build/configure the WCT graph now that the boundary TLAs
        // are injected.  initialize_ports() resolves virtually to this class.
        this->initialize_now();
    }

protected:
    static constexpr std::size_t n_in = sizeof...(Ins);
    static constexpr std::size_t n_out = sizeof...(Outs);

    // Fill each input source, run the WCT graph once, drain each output sink.
    std::tuple<Data<Outs>...> run(Data<Ins> const&... ins)
    {
        fill_all(std::forward_as_tuple(ins...), std::make_index_sequence<n_in>{});
        run_graph();
        return drain_all(std::make_index_sequence<n_out>{});
    }

    void initialize_ports() override
    {
        find_sources(std::make_index_sequence<n_in>{});
        find_sinks(std::make_index_sequence<n_out>{});
    }

    std::string src_name(std::size_t i) const { return port_source_name(m_scope, i); }
    std::string snk_name(std::size_t j) const { return port_sink_name(m_scope, j); }

    std::tuple<std::shared_ptr<BoundarySource<typename port_traits<Ins>::source_iface>>...> m_sources;
    std::tuple<std::shared_ptr<BoundarySink<typename port_traits<Outs>::sink_iface>>...> m_sinks;

private:
    template <std::size_t... I, std::size_t... J>
    void inject_tlas(std::index_sequence<I...>, std::index_sequence<J...>)
    {
        std::vector<std::pair<std::string, std::string>> sources{
            std::pair<std::string, std::string>{port_traits<Ins>::src_class, src_name(I)}...};
        std::vector<std::pair<std::string, std::string>> sinks{
            std::pair<std::string, std::string>{port_traits<Outs>::snk_class, snk_name(J)}...};
        inject_boundary_tlas(m_wcmain, sources, sinks);
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

// N -> 1 (homogeneous, DYNAMIC multiplicity) : a fan-in.  The port count is a
// runtime value (from configuration), so — unlike Join — it is NOT a template
// parameter and the port types are not a type_list.  The Phlex side deals in a
// std::vector<Data<In>>; the WCT side is a plain iteration into the per-port
// boundary sources.
template <class In, class Out>
class FaninExecutor : public Executor {
public:
    FaninExecutor(ExecutorConfig const& config, std::size_t multiplicity)
        : Executor(config)
        , m_mult(multiplicity)
    {
        std::vector<std::pair<std::string, std::string>> sources;
        for (std::size_t i = 0; i < m_mult; ++i) {
            sources.emplace_back(port_traits<In>::src_class, port_source_name(m_scope, i));
        }
        std::vector<std::pair<std::string, std::string>> sinks{
            {port_traits<Out>::snk_class, port_sink_name(m_scope, 0)}};
        inject_boundary_tlas(m_wcmain, sources, sinks);
        initialize_now();
    }

    Data<Out> operator()(std::vector<Data<In>> const& ins)
    {
        if (ins.size() != m_sources.size()) {
            throw std::runtime_error("FaninExecutor: input vector size " +
                                     std::to_string(ins.size()) + " != multiplicity " +
                                     std::to_string(m_sources.size()));
        }
        for (std::size_t i = 0; i < m_sources.size(); ++i) {
            m_sources[i]->fill(ins[i].ptr);
        }
        run_graph();
        return Data<Out>{m_sink->drain()};
    }

private:
    void initialize_ports() override
    {
        m_sources.resize(m_mult);
        for (std::size_t i = 0; i < m_mult; ++i) {
            m_sources[i] = find_boundary<typename port_traits<In>::source_iface,
                                         BoundarySource<typename port_traits<In>::source_iface>>(
                port_traits<In>::src_class, port_source_name(m_scope, i));
        }
        m_sink = find_boundary<typename port_traits<Out>::sink_iface,
                               BoundarySink<typename port_traits<Out>::sink_iface>>(
            port_traits<Out>::snk_class, port_sink_name(m_scope, 0));
    }

    std::size_t m_mult;
    std::vector<std::shared_ptr<BoundarySource<typename port_traits<In>::source_iface>>> m_sources;
    std::shared_ptr<BoundarySink<typename port_traits<Out>::sink_iface>> m_sink;
};

// 1 -> N (homogeneous, DYNAMIC multiplicity) : a fan-out.  Mirror of FaninExecutor;
// the Phlex side deals in a std::vector<Data<Out>>.
template <class In, class Out>
class FanoutExecutor : public Executor {
public:
    FanoutExecutor(ExecutorConfig const& config, std::size_t multiplicity)
        : Executor(config)
        , m_mult(multiplicity)
    {
        std::vector<std::pair<std::string, std::string>> sources{
            {port_traits<In>::src_class, port_source_name(m_scope, 0)}};
        std::vector<std::pair<std::string, std::string>> sinks;
        for (std::size_t j = 0; j < m_mult; ++j) {
            sinks.emplace_back(port_traits<Out>::snk_class, port_sink_name(m_scope, j));
        }
        inject_boundary_tlas(m_wcmain, sources, sinks);
        initialize_now();
    }

    std::vector<Data<Out>> operator()(Data<In> const& in)
    {
        m_source->fill(in.ptr);
        run_graph();
        std::vector<Data<Out>> outs;
        outs.reserve(m_sinks.size());
        for (auto const& snk : m_sinks) {
            outs.push_back(Data<Out>{snk->drain()});
        }
        return outs;
    }

private:
    void initialize_ports() override
    {
        m_source = find_boundary<typename port_traits<In>::source_iface,
                                 BoundarySource<typename port_traits<In>::source_iface>>(
            port_traits<In>::src_class, port_source_name(m_scope, 0));
        m_sinks.resize(m_mult);
        for (std::size_t j = 0; j < m_mult; ++j) {
            m_sinks[j] = find_boundary<typename port_traits<Out>::sink_iface,
                                       BoundarySink<typename port_traits<Out>::sink_iface>>(
                port_traits<Out>::snk_class, port_sink_name(m_scope, j));
        }
    }

    std::size_t m_mult;
    std::shared_ptr<BoundarySource<typename port_traits<In>::source_iface>> m_source;
    std::vector<std::shared_ptr<BoundarySink<typename port_traits<Out>::sink_iface>>> m_sinks;
};

// 0 -> 1 : a source (the WCT graph contains a real source, e.g. a file reader).
// Its execution protocol differs: the first call runs the graph to completion
// (queuing every output in the boundary sink); each call drains one.
template <class Out>
class SourceExecutor : public PortedExecutor<type_list<>, type_list<Out>> {
public:
    using PortedExecutor<type_list<>, type_list<Out>>::PortedExecutor;
    Data<Out> operator()()
    {
        // The WCT graph is built at construction; the real WCT source is driven
        // exactly once, on the first call (subsequent calls just drain).
        if (!m_ran.exchange(true)) {
            this->run_graph();
        }
        return Data<Out>{std::get<0>(this->m_sinks)->drain()};
    }

private:
    std::atomic<bool> m_ran{false};
};

// 1 -> collect : a transform whose WCT sub-graph emits a STREAM of Item (one per
// input sub-unit — e.g. one IBlobSet per time slice), collected into a single
// Phlex product DataVector<Item>.  Unlike FunctionExecutor (1 output drained per
// input) this drains the WHOLE boundary-sink queue after one graph run.  Derives
// directly from Executor (like the fans): one In boundary source, one Item
// boundary sink.  Used by the 3D-imaging island (IFrame -> vector<IBlobSet>).
template <class In, class Item>
class CollectExecutor : public Executor {
public:
    explicit CollectExecutor(ExecutorConfig const& config)
        : Executor(config)
    {
        std::vector<std::pair<std::string, std::string>> sources{
            {port_traits<In>::src_class, port_source_name(m_scope, 0)}};
        std::vector<std::pair<std::string, std::string>> sinks{
            {port_traits<Item>::snk_class, port_sink_name(m_scope, 0)}};
        inject_boundary_tlas(m_wcmain, sources, sinks);
        initialize_now();
    }

    // Fill the input source, run the WCT graph once, then drain every Item the
    // sub-graph produced (drain() returns nullptr when the queue is exhausted).
    DataVector<Item> operator()(Data<In> const& in)
    {
        m_source->fill(in.ptr);
        run_graph();
        DataVector<Item> out;
        while (auto item = m_sink->drain()) {
            out.items.push_back(item);
        }
        return out;
    }

private:
    void initialize_ports() override
    {
        m_source = find_boundary<typename port_traits<In>::source_iface,
                                 BoundarySource<typename port_traits<In>::source_iface>>(
            port_traits<In>::src_class, port_source_name(m_scope, 0));
        m_sink = find_boundary<typename port_traits<Item>::sink_iface,
                               BoundarySink<typename port_traits<Item>::sink_iface>>(
            port_traits<Item>::snk_class, port_sink_name(m_scope, 0));
    }

    std::shared_ptr<BoundarySource<typename port_traits<In>::source_iface>> m_source;
    std::shared_ptr<BoundarySink<typename port_traits<Item>::sink_iface>> m_sink;
};

} // namespace wcphlex
