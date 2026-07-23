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

// modules/deposet_to_frame.cpp
//
// PHLEX algorithm module: IDepoSet -> IFrame function node.
//
// Naming convention: <in>_to_<out> with <type> = the WCT IData type, "I"
// removed and lower-cased (IDepoSet -> deposet, IFrame -> frame).  Hence this
// file is deposet_to_frame.cpp, the module library is
// libwcph_deposet_to_frame.so, and the node is registered as
// "wcph_deposet_to_frame" (both derived by register_function()).
//
// The node is FunctionExecutor<IDepoSet,IFrame>: a 1-in/1-out transform backed
// by a WCT sub-graph (deposet-to-frame.jsonnet) whose data crosses the Phlex
// boundary through DepoSetBoundarySource / FrameBoundarySink.
//
// Config keys: wct_config (required), input_layer (required), input_from
// (required: "input" to consume a source, else an upstream module label),
// wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_function<WireCell::IDepoSet, WireCell::IFrame>(m, config);
}
