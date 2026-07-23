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

// modules/frame_filter.cpp
//
// PHLEX algorithm module: IFrame -> IFrame function node
// (FunctionExecutor<IFrame,IFrame>).  The data crosses the Phlex boundary
// through FrameBoundarySource / FrameBoundarySink.
//
// NOTE: the former use_wire_schema path (a 2-input transform on the original
// FrameFilter executor that side-channel-registered a WireSchema into
// FacadeWireSchema before WCT init) has been removed along with FrameFilter.
// Auxiliary wire-schema input will return via a join-executor-based mechanism;
// FacadeWireSchema is retained for that future work.
//
// Config keys: wct_config (required), inputs (1 selector), outputs (1),
// wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_function<WireCell::IFrame, WireCell::IFrame>(m, config);
}
