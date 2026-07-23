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

// modules/frames_to_frame_6.cpp
//
// PHLEX algorithm module: 6 x IFrame -> IFrame fan-in node
// (FaninExecutor<IFrame,IFrame,6>).  Node name "wcph_frames_to_frame_6".
//
// One of the frame fan-in multiplicity series (2, 3, 4, 6, 8).  The
// multiplicity is a compile-time template parameter because it fixes the Phlex
// node's argument arity, so one module .cpp is built per supported N (see
// modules/register_shapes.hpp).  Consumes 6 Frames (one per "inputs"
// selector), merges them (FrameFanin) and produces one merged Frame.
//
// Config keys: wct_config (required), inputs (6 selectors), outputs (1),
// wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_fanin<WireCell::IFrame, WireCell::IFrame, 6>(m, config);
}
