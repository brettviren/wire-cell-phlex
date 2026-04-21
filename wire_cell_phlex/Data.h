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

// wire_cell_phlex/Data.h
//
// Thin aggregate wrapper types that carry WCT immutable data pointers through
// PHLEX's typed product routing.
//
// WHY WRAPPERS: WCT's IData-derived interfaces are abstract classes.
// shared_ptr<const AbstractType> is not an aggregate and is not a contiguous
// container, so PHLEX's type_id system assigns it id_=0xFF (opaque).  Opaque
// types can still be stored in a product_store, but PHLEX cannot statically
// verify them; in practice any two shared_ptr-to-abstract fields look
// structurally identical (both id_=0x40, one child with id_=0xFF).
//
// The wrappers give each WCT concept its OWN C++ type.  PHLEX distinguishes
// them via dynamic_cast and typeid (the `exact_` field in type_id), so product
// retrieval is type-safe.  Routing by (creator, suffix) is sufficient for
// unambiguous lookup; the type acts as a safety check.
//
// All structs are C++20 aggregates (no user-declared constructors, no private
// members).

#pragma once

#include <WireCellIface/IDepo.h>
#include <WireCellIface/IDepoSet.h>
#include <WireCellIface/IFrame.h>
#include <WireCellIface/ITensor.h>
#include <WireCellIface/ITensorSet.h>
#include <WireCellUtil/WireSchema.h>

namespace wcphlex {

    // One ionisation deposit.
    struct Depo {
        WireCell::IDepo::pointer ptr;
    };

    // A set of deposits (one drift batch).
    struct DepoSet {
        WireCell::IDepoSet::pointer ptr;
    };

    // One readout frame (one drift window's worth of ADC/deconvolved waveforms).
    struct Frame {
        WireCell::IFrame::pointer ptr;
    };

    // A single named tensor.
    struct Tensor {
        WireCell::ITensor::pointer ptr;
    };

    // A set of named tensors.
    struct TensorSet {
        WireCell::ITensorSet::pointer ptr;
    };

    // Wire geometry schema loaded from a WCT wire file at the job layer.
    // WireCell::WireSchema::Store internally holds a shared_ptr to its data,
    // so copies are cheap.  The default-constructed store has a null pointer;
    // a successfully loaded store is guaranteed to have non-empty wires().
    struct WireSchema {
        WireCell::WireSchema::Store store;
    };

} // namespace wcphlex
