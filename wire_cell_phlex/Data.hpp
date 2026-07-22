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

    // Generic pointer-carrying wrapper, parameterised by the WCT interface type.
    // Each instantiation is a distinct C++ aggregate, so PHLEX distinguishes
    // products by typeid (the type_id exact_ field, see the note above):
    // Data<IFrame> and Data<IDepoSet> are distinct product types.  The templated
    // executors (FunctionExecutor<In,Out>, …) deal in Data<IType> directly.
    template <class IType>
    struct Data {
        typename IType::pointer ptr;
    };

    // The common WCT concepts as named aliases (convenience for module code).
    using Depo = Data<WireCell::IDepo>;              // one ionisation deposit
    using DepoSet = Data<WireCell::IDepoSet>;        // a set of deposits (a drift batch)
    using Frame = Data<WireCell::IFrame>;            // one readout frame
    using Tensor = Data<WireCell::ITensor>;          // a single named tensor
    using TensorSet = Data<WireCell::ITensorSet>;    // a set of named tensors

    // Wire geometry schema loaded from a WCT wire file at the job layer.  Kept a
    // distinct struct (not a Data<IType>): it carries a WireSchema::Store, not an
    // IType::pointer.  The Store internally holds a shared_ptr, so copies are
    // cheap; a default-constructed store is null, a loaded one has non-empty wires().
    struct WireSchema {
        WireCell::WireSchema::Store store;
    };

} // namespace wcphlex
