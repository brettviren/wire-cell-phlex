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

// wire_cell_phlex/BoundarySink.h
//
// Template for a WCT sink node that acts as the WCT→PHLEX boundary buffer.
//
// WCT calls operator() during wcmain() for each output item and once for EOS
// (in = nullptr).  After wcmain() returns, the PHLEX side calls drain() once
// per expected output item to retrieve results.
//
// Queue-based design: operator() enqueues all non-null inputs; drain() dequeues
// one item per call.  This handles graphs that produce multiple outputs per
// input as well as the typical single-item case.
//
// WIRECELL_FACTORY registrations for concrete types live in BoundaryNodes.cpp.

#pragma once

#include <WireCellIface/IConfigurable.h>
#include <queue>
#include <utility>

namespace wcphlex {

    // WctIface must be a WCT ISinkNode<T> subclass (e.g. WireCell::IFrameSink).
    // It provides:
    //   input_pointer  = std::shared_ptr<const T>
    //   operator()(const input_pointer& in)  (pure virtual — we implement it)
    template <typename WctIface>
    class BoundarySink : public WctIface, public WireCell::IConfigurable {
    public:
        using input_pointer = typename WctIface::input_pointer;

        BoundarySink() = default;
        virtual ~BoundarySink() = default;

        // ---- PHLEX-side interface ----------------------------------------

        // Dequeue and return the next output item produced during wcmain().
        // Returns nullptr when the queue is empty (all results consumed).
        input_pointer drain()
        {
            if (m_queue.empty()) {
                return nullptr;
            }
            auto item = std::move(m_queue.front());
            m_queue.pop();
            return item;
        }

        // ---- WCT sink interface ------------------------------------------

        // Called by the WCT graph during wcmain():
        //   non-null in: enqueue the output item.
        //   null in (EOS): silently ignored.
        //   Returns true (false = error/stop).
        bool operator()(input_pointer const& in) override
        {
            if (in) {
                m_queue.push(in);
            }
            return true;
        }

        // ---- IConfigurable (no-op: boundary nodes need no config) -------
        WireCell::Configuration default_configuration() const override
        {
            return WireCell::Configuration();
        }
        void configure(WireCell::Configuration const&) override {}

    private:
        std::queue<input_pointer> m_queue;
    };

} // namespace wcphlex
