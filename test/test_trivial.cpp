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

// test/test_trivial.cpp
//
// Step 1 smoke test: verifies the package builds and links against
// wire_cell_phlex without error.  No WCT graph or executor construction;
// just confirms that key headers are accessible and types are instantiable.

#include "wire_cell_phlex/Data.hpp"
#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"
#include "wire_cell_phlex/Executor.hpp"

#include <WireCellIface/IFrameSource.h>
#include <WireCellIface/IFrameSink.h>

int main()
{
    // Instantiate boundary types to confirm headers, vtable, and shared
    // library are reachable and link correctly.  No WCT initialization.
    wcphlex::BoundarySource<WireCell::IFrameSource> src;
    wcphlex::BoundarySink<WireCell::IFrameSink>     snk;
    (void)src;
    (void)snk;

    // Confirm wcphlex data wrapper types are constructible.
    wcphlex::Frame     f{nullptr};
    wcphlex::DepoSet   ds{nullptr};
    wcphlex::TensorSet ts{nullptr};
    (void)f; (void)ds; (void)ts;

    return 0;
}
