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

// modules/frame_fanin.cpp
//
// PHLEX algorithm module: N×IFrame -> IFrame fan-in node.
//
// A FaninExecutor<IFrame,IFrame> of fixed multiplicity 4: consumes four Frames
// (one per upstream creator), fills four GenericFrameBoundarySource nodes, runs
// a WCT sub-graph that merges them (FrameFanin) and drains the merged Frame
// through a GenericFrameBoundarySink.
//
// The multiplicity is a compile-time template constant: Phlex 0.3.2 retrieves
// exactly one product per input parameter, so a node's port count is its
// function arity (see modules/register_shapes.hpp).
//
// Config keys: wct_config (required), input_layer (required),
// input_from_0 .. input_from_3 (required: creator of each input Frame),
// input_suffix / output_suffix (optional, default "frame"),
// wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_fanin<WireCell::IFrame, WireCell::IFrame, 4>(m, config);
}
