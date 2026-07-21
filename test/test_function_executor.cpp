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

// test/test_function_executor.cpp  (Idea-2 spike, ddm-7dk.3)
//
// Proves the generic templated executor path end to end:
//   - BoundarySource<ISourceNode<IFrame>> / BoundarySink<ISinkNode<IFrame>>
//     register, are found, and wire through Pgraph (matching by data type);
//   - FunctionExecutor<IFrame,IFrame> round-trips a Frame with pointer identity;
//   - the product wrapper is the generic Data<IFrame>.
//
// Mirrors test_executor's FrameFilter test but with the templated executor and
// the generic (mid-level-interface) boundary nodes.

#include "wire_cell_phlex/FunctionExecutor.hpp"

#include <WireCellAux/SimpleFrame.h>
#include <WireCellIface/IFrame.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    std::string const cfg_path = std::string{WCP_CFG_DIR} + "/frame-passthrough-generic.jsonnet";

    // Build the executor config directly (no Phlex layer involved here).
    wcphlex::ExecutorConfig ec;
    ec.wct_config = cfg_path;
    ec.wct_plugins = std::vector<std::string>{"WireCellPgraph"};

    // using FrameFilter = FunctionExecutor<IFrame, IFrame>
    wcphlex::FunctionExecutor<WireCell::IFrame, WireCell::IFrame> ff{ec};
    std::cout << "FunctionExecutor<IFrame,IFrame> construction: PASS\n";

    // Single-event round-trip: pointer identity preserved through the WCT graph.
    {
        WireCell::IFrame::pointer frame = std::make_shared<WireCell::Aux::SimpleFrame>(42);
        auto result = ff(wcphlex::Data<WireCell::IFrame>{frame});
        assert(result.ptr && "result must be non-null");
        assert(result.ptr == frame && "pointer identity must be preserved");
        std::cout << "Single-event round-trip (ident=42): PASS\n";
    }

    // Re-use across events (the same WCT graph re-driven each call).
    {
        int constexpr N = 12;
        for (int i = 0; i < N; ++i) {
            WireCell::IFrame::pointer frame = std::make_shared<WireCell::Aux::SimpleFrame>(i);
            auto result = ff(wcphlex::Data<WireCell::IFrame>{frame});
            assert(result.ptr == frame && "pointer identity must be preserved");
        }
        std::cout << "12-event re-use: PASS\n";
    }

    std::cout << "All FunctionExecutor assertions passed.\n";
    return 0;
}
