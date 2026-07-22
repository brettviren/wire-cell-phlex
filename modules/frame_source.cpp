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

// modules/frame_source.cpp
//
// PHLEX provider module: (real WCT source) -> IFrame source node.
//
// A 0-in/1-out SourceExecutor<IFrame>: drives a WCT sub-graph that begins with
// a real WCT source and ends in a GenericFrameBoundarySink.  The particular
// source (e.g. FrameFileSource reading an npz) is chosen entirely by the
// wct_config — "read from a file" is just one such graph.  The first Phlex call
// runs the graph and queues every Frame; each call drains one.
//
// (Trivial in-memory Frames for connectivity tests come from the separate,
// pure-Phlex wcph_frame_gen module.)
//
// Config keys: wct_config (required), output_layer (required), output_suffix
// (optional, default "frame"), wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/source.hpp"

PHLEX_REGISTER_PROVIDERS(m, config)
{
    wcphlex::register_source<WireCell::IFrame>(m, config);
}
