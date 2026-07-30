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

// modules/tracksegmentset_to_frame.cpp
//
// PHLEX algorithm module: ITrackSegmentSet -> IFrame function node,
// registered as "wcph_tracksegmentset_to_frame".
//
// The full-chain WCT sub-graph runs Gen::TrackSegmentSampler then
// drift + detector simulation (+ optional sigproc) between the boundary
// nodes -- a tracksegmentset-input variant of the deposet-input job configs.
//
// Config keys: wct_config (required), input_layer (required), input_from
// (required), wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_function<WireCell::ITrackSegmentSet, WireCell::IFrame>(m, config);
}
