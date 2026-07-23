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

// wire_cell_phlex/BoundaryNodes.cpp
//
// WIRECELL_FACTORY registrations for the wcphlex boundary source and sink
// concrete types.  The static initializers run when wire_cell_phlex.so is
// loaded, making the factories available to WireCell::Main before initialize().
//
// The boundary nodes present the templated mid-level WCT interface
// (ISourceNode<IType> / ISinkNode<IType>) rather than a concrete per-type
// interface: Pgraph wires by data type, so this is all a shape executor needs,
// and one registration serves every node that carries that IData type.  The
// WCT type name is supplied to the Jsonnet config by the shape executor (via
// the sources/sinks inode arrays), so it is not hard-coded in any config.
//
// Concrete type names used in Jsonnet config (type: "Name"):
//   FrameBoundarySource / FrameBoundarySink
//   DepoSetBoundarySource / DepoSetBoundarySink

#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"

#include <WireCellIface/ISourceNode.h>
#include <WireCellIface/ISinkNode.h>
#include <WireCellIface/IFrame.h>
#include <WireCellIface/IDepoSet.h>
#include <WireCellUtil/NamedFactory.h>

// --- Frame boundary nodes ---------------------------------------------------

WIRECELL_FACTORY(FrameBoundarySource,
                 wcphlex::BoundarySource<WireCell::ISourceNode<WireCell::IFrame>>,
                 WireCell::ISourceNode<WireCell::IFrame>,
                 WireCell::IConfigurable)

WIRECELL_FACTORY(FrameBoundarySink,
                 wcphlex::BoundarySink<WireCell::ISinkNode<WireCell::IFrame>>,
                 WireCell::ISinkNode<WireCell::IFrame>,
                 WireCell::IConfigurable)

// --- DepoSet boundary nodes -------------------------------------------------

WIRECELL_FACTORY(DepoSetBoundarySource,
                 wcphlex::BoundarySource<WireCell::ISourceNode<WireCell::IDepoSet>>,
                 WireCell::ISourceNode<WireCell::IDepoSet>,
                 WireCell::IConfigurable)

WIRECELL_FACTORY(DepoSetBoundarySink,
                 wcphlex::BoundarySink<WireCell::ISinkNode<WireCell::IDepoSet>>,
                 WireCell::ISinkNode<WireCell::IDepoSet>,
                 WireCell::IConfigurable)
