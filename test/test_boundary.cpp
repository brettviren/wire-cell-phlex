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
// WCT graph.  Queue-based BoundarySource behaviour:
//
//   fill(data)        → enqueues data, resets EOS state
//   src(out) call 1   → out = data,    true   (dequeued from queue)
//   src(out) call 2   → out = nullptr, true   (EOS signal)
//   src(out) call 3   → out = nullptr, false  (graph terminates)
//
// Tests:
//   1. Single fill + three operator() calls on Frame source/sink.
//   2. Re-use: after the third call returns false, a new fill() resets state
//      and the cycle repeats correctly (5 rounds).
//   3. Initial state: unfilled source returns EOS on first call (empty queue).
//   4. DepoSet variants behave identically.
//   5. Multi-fill: two fill() calls before execution serve both items then EOS.

#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"

#include <WireCellAux/SimpleFrame.h>
#include <WireCellAux/SimpleDepoSet.h>
#include <WireCellIface/IFrameSource.h>
#include <WireCellIface/IFrameSink.h>
#include <WireCellIface/IDepoSetSource.h>
#include <WireCellIface/IDepoSetSink.h>

// Ensure assert() is active even in RelWithDebInfo builds (which define NDEBUG).
#undef NDEBUG
#include <cassert>
#include <iostream>
#include <memory>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static WireCell::IFrame::pointer make_frame(int ident)
{
    return std::make_shared<WireCell::Aux::SimpleFrame>(ident);
}

static WireCell::IDepoSet::pointer make_deposet(int ident)
{
    return std::make_shared<WireCell::Aux::SimpleDepoSet>(ident, WireCell::IDepo::vector{});
}

// ---------------------------------------------------------------------------
// run_one_round: fill once, simulate three WCT source calls, sink round-trip.
// ---------------------------------------------------------------------------

template <typename SourceIface, typename SinkIface, typename Ptr>
void run_one_round(wcphlex::BoundarySource<SourceIface>& src,
                   wcphlex::BoundarySink<SinkIface>&    snk,
                   Ptr                                  data)
{
    using output_pointer = typename SourceIface::output_pointer;
    using input_pointer  = typename SinkIface::input_pointer;

    // ---- PHLEX side: fill source with one item ---------------------------
    src.fill(data);

    // ---- Simulate WCT graph calling source three times ------------------
    output_pointer out1;
    assert(src(out1)     && "source must return true on first call (data)");
    assert(out1          && "source must return data on first call");
    assert(out1 == data  && "source must return exactly the filled pointer");

    output_pointer out2;
    assert(src(out2)     && "source must return true on EOS call");
    assert(!out2         && "source must return nullptr as EOS signal");

    // Third call: source must return false to let Pgraph terminate the graph.
    output_pointer out3;
    assert(!src(out3)    && "source must return false after EOS to stop graph");
    assert(!out3         && "source must return nullptr on termination call");

    // ---- Simulate WCT graph calling sink with data then EOS -------------
    assert(snk(input_pointer(out1))    && "sink must return true on data");
    assert(snk(input_pointer(nullptr)) && "sink must return true on EOS");

    // ---- PHLEX side: drain result ----------------------------------------
    auto result = snk.drain();
    assert(result        && "drain must return a non-null result");
    assert(result == data && "drain must return exactly the original pointer");

    // After draining the one item, queue is empty.
    auto empty = snk.drain();
    assert(!empty && "second drain must return nullptr");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
    // --- Test 1 & 2: Frame source/sink single round-trip ------------------
    {
        wcphlex::BoundarySource<WireCell::IFrameSource> src;
        wcphlex::BoundarySink<WireCell::IFrameSink>     snk;

        auto frame = make_frame(42);
        run_one_round(src, snk, frame);
        std::cout << "Frame round-trip (ident=42): PASS\n";
    }

    // --- Test 3: re-use across 5 events -----------------------------------
    {
        wcphlex::BoundarySource<WireCell::IFrameSource> src;
        wcphlex::BoundarySink<WireCell::IFrameSink>     snk;

        for (int i = 0; i < 5; ++i) {
            auto frame = make_frame(i);
            run_one_round(src, snk, frame);
        }
        std::cout << "Frame re-use (5 rounds): PASS\n";
    }

    // --- Test 4: initial state — unfilled source returns EOS immediately --
    {
        wcphlex::BoundarySource<WireCell::IFrameSource> src;
        WireCell::IFrame::pointer out;
        assert(src(out)  && "unfilled source must return true for first (EOS) call");
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

    // --- Test 6: multi-fill (two items per event) -------------------------
    {
        wcphlex::BoundarySource<WireCell::IFrameSource> src;

        auto f0 = make_frame(10);
        auto f1 = make_frame(11);

        src.fill(f0);
        src.fill(f1);

        WireCell::IFrame::pointer out;

        assert(src(out) && out == f0  && "multi-fill: first item");
        assert(src(out) && out == f1  && "multi-fill: second item");
        assert(src(out) && !out       && "multi-fill: EOS after both items");
        assert(!src(out) && !out      && "multi-fill: false after EOS");

        std::cout << "Multi-fill (2 items): PASS\n";
    }

    std::cout << "All boundary assertions passed.\n";
    return 0;
}
