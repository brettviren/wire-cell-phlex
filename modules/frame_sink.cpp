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

// modules/frame_sink.cpp
//
// PHLEX algorithm module: IFrame -> (terminal) sink node.
//
// A 1-in/0-out SinkExecutor<IFrame>: consumes each Frame, fills the WCT
// FrameBoundarySource, and runs a WCT sub-graph terminating in a real
// WCT sink.  The particular sink (e.g. FrameFileSink writing an npz) is chosen
// entirely by the wct_config — "sink to a file" is just one such graph.
//
// Config keys: wct_config (required), input_layer (required), input_from
// (required: "input" to consume a source, else an upstream module label),
// input_suffix (optional, default "frame"), wct_plugins / wct_app / wct_tla
// (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_sink<WireCell::IFrame>(m, config);
}
