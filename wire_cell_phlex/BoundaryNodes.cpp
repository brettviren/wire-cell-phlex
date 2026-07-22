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
// WIRECELL_FACTORY registrations for all wcphlex boundary source and sink
// concrete types.  The static initializers run when wire_cell_phlex.so is
// loaded, making the factories available to WireCell::Main before initialize().
//
// Concrete type names used in Jsonnet config (type: "Name"):
//   FrameBoundarySource / FrameBoundarySink
//   DepoSetBoundarySource / DepoSetBoundarySink
//   TensorSetBoundarySource / TensorSetBoundarySink

#include "wire_cell_phlex/BoundarySource.hpp"
#include "wire_cell_phlex/BoundarySink.hpp"

#include <WireCellIface/ISourceNode.h>
#include <WireCellIface/ISinkNode.h>
#include <WireCellIface/IFrameSource.h>
#include <WireCellIface/IFrameSink.h>
#include <WireCellIface/IDepoSetSource.h>
#include <WireCellIface/IDepoSetSink.h>
#include <WireCellIface/ITensorSetSource.h>
#include <WireCellIface/ITensorSetSink.h>
#include <WireCellUtil/NamedFactory.h>

// --- Frame boundary nodes ---------------------------------------------------

WIRECELL_FACTORY(FrameBoundarySource,
                 wcphlex::BoundarySource<WireCell::IFrameSource>,
                 WireCell::IFrameSource,
                 WireCell::IConfigurable)

WIRECELL_FACTORY(FrameBoundarySink,
                 wcphlex::BoundarySink<WireCell::IFrameSink>,
                 WireCell::IFrameSink,
                 WireCell::IConfigurable)

// --- Generic (mid-level-interface) Frame boundary nodes (Idea-2 spike) ------
// Same behavior, but the WCT interface is the templated ISourceNode<IFrame> /
// ISinkNode<IFrame> rather than the concrete IFrameSource / IFrameSink.  This
// is what FunctionExecutor<IFrame,IFrame> finds; it proves the generic
// mid-level interface wires through Pgraph (which matches by data type).
WIRECELL_FACTORY(GenericFrameBoundarySource,
                 wcphlex::BoundarySource<WireCell::ISourceNode<WireCell::IFrame>>,
                 WireCell::ISourceNode<WireCell::IFrame>,
                 WireCell::IConfigurable)

WIRECELL_FACTORY(GenericFrameBoundarySink,
                 wcphlex::BoundarySink<WireCell::ISinkNode<WireCell::IFrame>>,
                 WireCell::ISinkNode<WireCell::IFrame>,
                 WireCell::IConfigurable)

// --- DepoSet boundary nodes -------------------------------------------------

WIRECELL_FACTORY(DepoSetBoundarySource,
                 wcphlex::BoundarySource<WireCell::IDepoSetSource>,
                 WireCell::IDepoSetSource,
                 WireCell::IConfigurable)

WIRECELL_FACTORY(DepoSetBoundarySink,
                 wcphlex::BoundarySink<WireCell::IDepoSetSink>,
                 WireCell::IDepoSetSink,
                 WireCell::IConfigurable)

// Generic (mid-level-interface) DepoSet boundary nodes — used by the templated
// shape executors (FunctionExecutor<IDepoSet,...> etc.).
WIRECELL_FACTORY(GenericDepoSetBoundarySource,
                 wcphlex::BoundarySource<WireCell::ISourceNode<WireCell::IDepoSet>>,
                 WireCell::ISourceNode<WireCell::IDepoSet>,
                 WireCell::IConfigurable)

WIRECELL_FACTORY(GenericDepoSetBoundarySink,
                 wcphlex::BoundarySink<WireCell::ISinkNode<WireCell::IDepoSet>>,
                 WireCell::ISinkNode<WireCell::IDepoSet>,
                 WireCell::IConfigurable)

// --- TensorSet boundary nodes -----------------------------------------------

WIRECELL_FACTORY(TensorSetBoundarySource,
                 wcphlex::BoundarySource<WireCell::ITensorSetSource>,
                 WireCell::ITensorSetSource,
                 WireCell::IConfigurable)

WIRECELL_FACTORY(TensorSetBoundarySink,
                 wcphlex::BoundarySink<WireCell::ITensorSetSink>,
                 WireCell::ITensorSetSink,
                 WireCell::IConfigurable)
