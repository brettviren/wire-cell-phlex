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
// WCT calls operator() during wcmain() for each output item and once
// for EOS (in = nullptr).  After wcmain() returns, the PHLEX side calls
// drain() to retrieve the accumulated output.
//
// For single-item sinks (one frame per event) drain() returns the last
// non-null item received; any subsequent EOS call is silently ignored.
// WIRECELL_FACTORY registrations for concrete types live in BoundaryNodes.cpp.

#pragma once

#include <WireCellIface/IConfigurable.h>
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

        // Retrieve and clear the accumulated output after wcmain().
        // Returns nullptr if no data was produced (should not happen in normal use).
        input_pointer drain()
        {
            return std::exchange(m_data, nullptr);
        }

        // ---- WCT sink interface ------------------------------------------

        // Called by the WCT graph during wcmain().
        //   non-null in: store the output item.
        //   null in (EOS): silently ignored.
        //   Returns true (false = error/stop).
        bool operator()(input_pointer const& in) override
        {
            if (in) {
                m_data = in;
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
        input_pointer m_data{};
    };

} // namespace wcphlex
