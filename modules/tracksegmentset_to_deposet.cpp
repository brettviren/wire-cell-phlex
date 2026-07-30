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

// modules/tracksegmentset_to_deposet.cpp
//
// PHLEX algorithm module: ITrackSegmentSet -> IDepoSet function node,
// registered as "wcph_tracksegmentset_to_deposet".
//
// The canonical WCT sub-graph runs Gen::TrackSegmentSampler between the
// boundary nodes (cfg/tracksegmentset-sampler.jsonnet), converting energy
// deposit segments into point depos with an ionization model.
//
// Config keys: wct_config (required), input_layer (required), input_from
// (required), wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_function<WireCell::ITrackSegmentSet, WireCell::IDepoSet>(m, config);
}
