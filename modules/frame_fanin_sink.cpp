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

// modules/frame_fanin_sink.cpp
//
// PHLEX module registering TWO chained nodes in one PHLEX_REGISTER_ALGORITHMS:
// a fan-in (N×IFrame -> IFrame) followed by a terminal sink (IFrame -> file).
// It is exactly the composition of the frame_fanin and frame_sink module bodies
// — the merged Frame the fan-in produces is consumed by the sink as an ordinary
// intra-module PHLEX product (creator = this module's label).
//
// Config: two sub-blocks, each the flat config the corresponding standalone
// module would take, e.g.
//   fanin: { module_label, wct_config, input_layer, input_from_0..3, ... }
//   sink:  { module_label, wct_config, input_layer, input_from, wct_tla, ... }
// Each sub-block carries its own module_label so the two Executors get distinct
// WCT component scopes (the top-level module_label is injected only once, at the
// module level, and drives the PHLEX product creator).  The sink's input_from
// is this module's own (top-level) label, since that is the fan-in's creator.

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_fanin<WireCell::IFrame, WireCell::IFrame, 4>(
        m, config.get<phlex::configuration>("fanin"));
    wcphlex::register_sink<WireCell::IFrame>(
        m, config.get<phlex::configuration>("sink"));
}
