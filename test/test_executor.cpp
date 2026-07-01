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

// test/test_executor.cpp
//
// Step 4: unit test for FrameFilter executor with cfg/frame-passthrough.jsonnet.
//
// Tests:
//   1. FrameFilter constructs without throwing.
//   2. Single-event round-trip: input Frame pointer is preserved through the
//      WCT pass-through graph.
//   3. 12-event re-use: each call produces the correct output, verifying the
//      per-event fresh-WireCell::Main protocol.
//
// The test requires one compile-time constant injected by CMakeLists.txt:
//   WCP_CFG_DIR  — absolute path to the cfg/ directory (contains *.jsonnet)
//
// The WireCellPgraph plugin provides the Pgrapher IApplication used by
// FrameFilter; it is loaded at runtime via WireCell::Main::add_plugin.

#include "wire_cell_phlex/Executor.hpp"

#include <WireCellAux/SimpleFrame.h>
#include <WireCellAux/SimpleDepoSet.h>
#include <WireCellIface/IFrame.h>
#include <WireCellIface/IDepoSet.h>

#include <boost/json.hpp>

#include <cassert>
#include <iostream>
#include <string>

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
// Main
// ---------------------------------------------------------------------------

int main()
{
    // Absolute paths to Jsonnet configs, injected at compile time.
    std::string const frame_cfg_path =
        std::string{WCP_CFG_DIR} + "/frame-passthrough.jsonnet";
    std::string const deposet_cfg_path =
        std::string{WCP_CFG_DIR} + "/deposet-passthrough.jsonnet";

    // Keep old name for backward compat with FrameFilter tests below.
    std::string const& cfg_path = frame_cfg_path;

    // Build the executor config as a boost::json::object.
    // (phlex::configuration is not used here: its header pulls in
    //  product_selector.hpp which uses std::forward_like, absent in GCC 12.)
    boost::json::object config{
        {"wct_config",  cfg_path},
        {"wct_plugins", boost::json::array{"WireCellPgraph"}},
    };

    // --- Test 1: construction -----------------------------------------------
    wcphlex::FrameFilter ff{config};
    std::cout << "FrameFilter construction: PASS\n";

    // --- Test 2: single-event round-trip ------------------------------------
    {
        auto frame  = make_frame(42);
        auto result = ff(wcphlex::Frame{frame});

        assert(result.ptr          && "result must be non-null");
        assert(result.ptr == frame && "pointer identity must be preserved");
        std::cout << "Single-event round-trip (ident=42): PASS\n";
    }

    // --- Test 3: 12-event re-use --------------------------------------------
    {
        int constexpr N = 12;
        for (int i = 0; i < N; ++i) {
            auto frame  = make_frame(i);
            auto result = ff(wcphlex::Frame{frame});

            assert(result.ptr          && "result must be non-null");
            assert(result.ptr == frame && "pointer identity must be preserved");
        }
        std::cout << "12-event re-use (" << 12 << " events): PASS\n";
    }

    // =========================================================================
    // DepoSetFilter tests
    // =========================================================================

    boost::json::object ds_config{
        {"wct_config",  deposet_cfg_path},
        {"wct_plugins", boost::json::array{"WireCellPgraph"}},
    };

    // --- Test 4: DepoSetFilter construction ---------------------------------
    wcphlex::DepoSetFilter dsf{ds_config};
    std::cout << "DepoSetFilter construction: PASS\n";

    // --- Test 5: single-event round-trip ------------------------------------
    {
        auto deposet = make_deposet(42);
        auto result  = dsf(wcphlex::DepoSet{deposet});

        assert(result.ptr            && "result must be non-null");
        assert(result.ptr == deposet && "pointer identity must be preserved");
        std::cout << "DepoSetFilter single-event round-trip (ident=42): PASS\n";
    }

    // --- Test 6: 12-event re-use --------------------------------------------
    {
        int constexpr N = 12;
        for (int i = 0; i < N; ++i) {
            auto deposet = make_deposet(i);
            auto result  = dsf(wcphlex::DepoSet{deposet});

            assert(result.ptr            && "result must be non-null");
            assert(result.ptr == deposet && "pointer identity must be preserved");
        }
        std::cout << "DepoSetFilter 12-event re-use (" << 12 << " events): PASS\n";
    }

    std::cout << "All executor assertions passed.\n";
    return 0;
}
