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

// test/test_boundary.cpp
//
// Step 3: unit test for BoundarySource and BoundarySink.
//
// Tests the PHLEX/WCT boundary buffer classes directly, without running a
// WCT graph.  Checks:
//   1. BoundarySource serves one data item then EOS.
//   2. BoundarySink accumulates a single item; drain() returns it.
//   3. Round-trip: pointer identity is preserved through fill → op() → drain.
//   4. Re-use: fill() resets state; a second round-trip works correctly.
//   5. DepoSet variants behave identically to Frame variants.

#include "wire_cell_phlex/BoundarySource.h"
#include "wire_cell_phlex/BoundarySink.h"

#include <WireCellAux/SimpleFrame.h>
#include <WireCellAux/SimpleDepoSet.h>
#include <WireCellIface/IFrameSource.h>
#include <WireCellIface/IFrameSink.h>
#include <WireCellIface/IDepoSetSource.h>
#include <WireCellIface/IDepoSetSink.h>

#include <cassert>
#include <iostream>
#include <memory>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Make a minimal IFrame with a given ident.
static WireCell::IFrame::pointer make_frame(int ident)
{
    return std::make_shared<WireCell::Aux::SimpleFrame>(ident);
}

// Make a minimal IDepoSet with a given ident.
static WireCell::IDepoSet::pointer make_deposet(int ident)
{
    return std::make_shared<WireCell::Aux::SimpleDepoSet>(ident, WireCell::IDepo::vector{});
}

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

template <typename SourceIface, typename SinkIface, typename Ptr>
void run_one_round(wcphlex::BoundarySource<SourceIface>& src,
                   wcphlex::BoundarySink<SinkIface>&    snk,
                   Ptr                                  data)
{
    using output_pointer = typename SourceIface::output_pointer;
    using input_pointer  = typename SinkIface::input_pointer;

    // ---- PHLEX side: fill source ------------------------------------------
    src.fill(data);

    // ---- Simulate WCT graph calling source twice --------------------------
    output_pointer out1;
    bool ok1 = src(out1);
    assert(ok1                && "source must return true on first call");
    assert(out1               && "source must return data on first call");
    assert(out1 == data       && "source must return exactly the filled pointer");

    output_pointer out2;
    bool ok2 = src(out2);
    assert(ok2                && "source must return true on EOS call");
    assert(!out2              && "source must return nullptr as EOS signal");

    // ---- Simulate WCT graph calling sink with data then EOS ---------------
    bool s1 = snk(input_pointer(out1));   // deliver data (same ptr)
    assert(s1 && "sink must return true on data");

    bool s2 = snk(input_pointer(nullptr)); // EOS
    assert(s2 && "sink must return true on EOS");

    // ---- PHLEX side: drain result -----------------------------------------
    auto result = snk.drain();
    assert(result       && "drain must return a non-null result");
    assert(result == data && "drain must return exactly the original pointer");

    // After drain, the sink is clear.
    auto empty = snk.drain();
    assert(!empty && "second drain must return nullptr");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
    // --- Test 1 & 2: Frame source/sink round-trip --------------------------
    {
        wcphlex::BoundarySource<WireCell::IFrameSource> src;
        wcphlex::BoundarySink<WireCell::IFrameSink>     snk;

        auto frame = make_frame(42);
        run_one_round(src, snk, frame);
        std::cout << "Frame round-trip (ident=42): PASS\n";
    }

    // --- Test 3: BoundarySource is re-usable across events -----------------
    {
        wcphlex::BoundarySource<WireCell::IFrameSource> src;
        wcphlex::BoundarySink<WireCell::IFrameSink>     snk;

        for (int i = 0; i < 5; ++i) {
            auto frame = make_frame(i);
            run_one_round(src, snk, frame);
        }
        std::cout << "Frame re-use (5 rounds): PASS\n";
    }

    // --- Test 4: Initial state — source returns EOS immediately if not filled
    {
        wcphlex::BoundarySource<WireCell::IFrameSource> src;
        WireCell::IFrame::pointer out;
        bool ok = src(out);
        assert(ok  && "unfilled source must still return true");
        assert(!out && "unfilled source must return nullptr");
        std::cout << "Unfilled source returns EOS immediately: PASS\n";
    }

    // --- Test 5: DepoSet source/sink round-trip ----------------------------
    {
        wcphlex::BoundarySource<WireCell::IDepoSetSource> src;
        wcphlex::BoundarySink<WireCell::IDepoSetSink>     snk;

        auto ds = make_deposet(7);
        run_one_round(src, snk, ds);
        std::cout << "DepoSet round-trip (ident=7): PASS\n";
    }

    std::cout << "All boundary assertions passed.\n";
    return 0;
}
