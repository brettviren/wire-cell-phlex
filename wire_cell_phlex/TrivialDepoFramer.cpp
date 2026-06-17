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

// wire_cell_phlex/TrivialDepoFramer.cpp

#include "wire_cell_phlex/TrivialDepoFramer.hpp"

#include <WireCellAux/SimpleFrame.h>
#include <WireCellUtil/NamedFactory.h>

WIRECELL_FACTORY(TrivialDepoFramer, wcphlex::TrivialDepoFramer,
                 WireCell::IDepoFramer,
                 WireCell::IConfigurable)

namespace wcphlex {

bool TrivialDepoFramer::operator()(input_pointer const& in, output_pointer& out)
{
    if (!in) {
        out = nullptr;  // propagate EOS to downstream FrameBoundarySink
        return true;
    }
    // Produce an empty frame whose ident matches the incoming DepoSet ident.
    out = std::make_shared<WireCell::Aux::SimpleFrame>(in->ident());
    return true;
}

WireCell::Configuration TrivialDepoFramer::default_configuration() const
{
    return WireCell::Configuration();
}

void TrivialDepoFramer::configure(WireCell::Configuration const&)
{
    // no configuration needed
}

} // namespace wcphlex
