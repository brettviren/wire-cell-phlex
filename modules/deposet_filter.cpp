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

// modules/deposet_filter.cpp
//
// PHLEX algorithm module: IDepoSet -> IDepoSet function node.
//
// A 1-in/1-out transform backed by a WCT sub-graph (e.g. an identity
// passthrough or a DepoSetDrifter) whose data crosses the Phlex boundary
// through DepoSetBoundarySource / DepoSetBoundarySink.  The node
// is FunctionExecutor<IDepoSet,IDepoSet>, registered via register_function().
//
// The library keeps its historical name (libwcph_deposet_filter.so) although
// the naming convention's <in>_to_<out> node name is wcph_deposet_to_deposet.
//
// Config keys: wct_config (required), input_layer (required), input_from
// (required: "input" to consume a source, else an upstream module label),
// input_suffix / output_suffix (optional, default "deposet"),
// wct_plugins / wct_app / wct_tla (optional).

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_function<WireCell::IDepoSet, WireCell::IDepoSet>(m, config);
}
