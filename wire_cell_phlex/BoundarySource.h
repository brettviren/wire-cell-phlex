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

// wire_cell_phlex/BoundarySource.h
//
// Template for a WCT source node that acts as the PHLEX→WCT boundary buffer.
//
// Usage pattern (persistent graph, repeated per PHLEX event):
//   1. source->fill(wct_ptr)      // PHLEX side: enqueue one event's data
//   2. wcmain()                   // run WCT graph (may be called once per event)
//   3. result = sink->drain()     // PHLEX side: dequeue output
//
// Queue-based design (mirrors larwirecell SimDepoSource):
//   - fill() pushes items onto an internal queue and resets EOS state.
//   - WCT's graph calls operator() repeatedly until no node does any work:
//       · Queue non-empty → dequeue item, return (item, true)
//       · Queue empty, EOS not yet sent → return (nullptr, true)  [EOS signal]
//       · Queue empty, EOS sent → return (nullptr, false)         [graph stops]
//   - After operator() returns false, the Pgraph::Source wrapper still calls
//     the underlying node on every subsequent execute() pass (there is no
//     persistent "skip if m_ok=false" guard in Pgraph::Source).  A new fill()
//     before the next wcmain() call refills the queue and resets m_eos_sent,
//     so the next graph execution processes fresh data correctly.
//
// Multiple items per event are supported: call fill() several times before
// wcmain(); the source will dequeue them in order, then send EOS.
//
// WIRECELL_FACTORY registrations for concrete types live in BoundaryNodes.cpp.

#pragma once

#include <WireCellIface/IConfigurable.h>
#include <queue>
#include <utility>

namespace wcphlex {

    // WctIface must be a WCT ISourceNode<T> subclass (e.g. WireCell::IFrameSource).
    // It provides:
    //   output_pointer  = std::shared_ptr<const T>
    //   operator()(output_pointer& out)  (pure virtual — we implement it)
    template <typename WctIface>
    class BoundarySource : public WctIface, public WireCell::IConfigurable {
    public:
        using output_pointer = typename WctIface::output_pointer;

        BoundarySource() = default;
        virtual ~BoundarySource() = default;

        // ---- PHLEX-side interface ----------------------------------------

        // Enqueue one item for the next WCT graph execution.
        // Also resets the EOS-sent flag so this source will run again.
        // May be called multiple times before wcmain() to push a batch.
        void fill(output_pointer data)
        {
            m_queue.push(std::move(data));
            m_eos_sent = false;
        }

        // ---- WCT source interface ----------------------------------------

        // Called repeatedly by the WCT graph during wcmain():
        //   · Queue non-empty:              out = next item,  returns true
        //   · Queue empty, EOS not sent:    out = nullptr,    returns true (EOS)
        //   · Queue empty, EOS already sent: out = nullptr,   returns false (done)
        bool operator()(output_pointer& out) override
        {
            if (!m_queue.empty()) {
                out = std::move(m_queue.front());
                m_queue.pop();
                return true;
            }
            if (!m_eos_sent) {
                out = nullptr;       // EOS signal
                m_eos_sent = true;
                return true;
            }
            out = nullptr;
            return false;            // graph may stop calling this source
        }

        // ---- IConfigurable (no-op: boundary nodes need no config) -------
        WireCell::Configuration default_configuration() const override
        {
            return WireCell::Configuration();
        }
        void configure(WireCell::Configuration const&) override {}

    private:
        std::queue<output_pointer> m_queue;
        bool m_eos_sent{false};
    };

} // namespace wcphlex
