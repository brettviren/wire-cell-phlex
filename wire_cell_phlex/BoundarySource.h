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
// Usage pattern (per PHLEX event):
//   1.  source->fill(wct_ptr)      // PHLEX side: load one event's data
//   2.  wcmain()                   // run WCT graph
//   3.  result = sink->drain()     // PHLEX side: retrieve output
//
// WCT calls operator() during step 2:
//   - First call:  out = m_data (non-null), returns true
//   - Second call: out = nullptr (EOS), returns true
//
// The node is re-usable: a subsequent fill() resets the served flag.
// WIRECELL_FACTORY registrations for concrete types live in BoundaryNodes.cpp.

#pragma once

#include <WireCellIface/IConfigurable.h>
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

        // Pre-load one event's data before calling wcmain().
        // Resets the served flag so the next WCT call gets fresh data.
        void fill(output_pointer data)
        {
            m_data = std::move(data);
            m_served = false;
        }

        // ---- WCT source interface ----------------------------------------

        // Called by the WCT graph during wcmain().
        //   First call:  delivers m_data and marks it served.
        //   Second call: sends EOS (out = nullptr).
        //   Returns true in both cases (false = error/stop).
        bool operator()(output_pointer& out) override
        {
            if (!m_served) {
                out = m_data;
                m_served = true;
            }
            else {
                out = nullptr;  // EOS signal
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
        output_pointer m_data{};
        bool m_served{true};  // true → nothing to serve yet
    };

} // namespace wcphlex
