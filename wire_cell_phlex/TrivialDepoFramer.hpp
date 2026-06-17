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

#pragma once

// wire_cell_phlex/TrivialDepoFramer.h
//
// Minimal WCT IDepoFramer implementation for integration testing.
//
// Converts each IDepoSet to an empty IFrame whose ident matches the
// DepoSet ident.  No drift, no field response, no electronics simulation —
// purely a connectivity test to exercise the DepoSetBoundarySource →
// (graph) → FrameBoundarySink path driven by wcphlex::DepoSetToFrame.

#include <WireCellIface/IDepoFramer.h>
#include <WireCellIface/IConfigurable.h>

namespace wcphlex {

class TrivialDepoFramer : public WireCell::IDepoFramer,
                          public WireCell::IConfigurable {
public:
    TrivialDepoFramer() = default;
    virtual ~TrivialDepoFramer() = default;

    // IDepoFramer (IFunctionNode<IDepoSet, IFrame>)
    bool operator()(input_pointer const& in, output_pointer& out) override;

    // IConfigurable
    WireCell::Configuration default_configuration() const override;
    void configure(WireCell::Configuration const& cfg) override;
};

} // namespace wcphlex
