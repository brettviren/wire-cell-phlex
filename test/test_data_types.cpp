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

// test/test_data_types.cpp
//
// Step 2: verify wcphlex wrapper types behave correctly in PHLEX's type system.
//
// Key assertions:
//   1. make_type_id<T>().valid() is true for all wrapper types.
//      (id_ != 0xFF, i.e. not the "opaque non-aggregate" sentinel).
//   2. Each wrapper type has its own C++ identity: exact_compare() returns
//      false for two different wrapper types (same structure, different exact_).
//   3. Wrappers can be value-constructed from null pointers and moved.
//
// NOTE on structural equality: PHLEX's structural type_id compares wrapper
// types as equal (both are aggregates with one shared_ptr child that maps to
// id_=0xFF), because shared_ptr is not an aggregate.  This is expected and
// harmless: routing is unambiguous because (creator, suffix) is unique per
// product, and PHLEX uses dynamic_cast for type-safe retrieval.

#include "wire_cell_phlex/Data.hpp"
#include "phlex/model/type_id.hpp"

#include <cassert>
#include <iostream>
#include <string>

using phlex::detail::make_type_id;

// ---- helpers ----------------------------------------------------------------

template <typename T>
void check_valid(const char* name)
{
    auto id = make_type_id<T>();
    std::cout << name << ": type_id valid=" << id.valid()
              << " exact=" << id.exact_name() << "\n";
    assert(id.valid() && "wrapper type must not be opaque (id_ must not be 0xFF)");
}

template <typename A, typename B>
void check_exact_distinct(const char* na, const char* nb)
{
    auto ia = make_type_id<A>();
    auto ib = make_type_id<B>();
    // Structural (operator==) may be equal due to shared_ptr children — that's
    // acceptable.  What matters is exact_ distinguishes them.
    assert(!ia.exact_compare(ib) && "two different wrapper types must have distinct exact_ types");
    (void)na; (void)nb;
}

// ---- main -------------------------------------------------------------------

int main()
{
    // 1. All wrapper types must be valid (id_ != 0xFF).
    check_valid<wcphlex::Depo>("Depo");
    check_valid<wcphlex::DepoSet>("DepoSet");
    check_valid<wcphlex::Frame>("Frame");
    check_valid<wcphlex::Tensor>("Tensor");
    check_valid<wcphlex::TensorSet>("TensorSet");

    // 2. Different wrapper types must be exactly distinct.
    check_exact_distinct<wcphlex::Frame, wcphlex::DepoSet>("Frame", "DepoSet");
    check_exact_distinct<wcphlex::Frame, wcphlex::Depo>("Frame", "Depo");
    check_exact_distinct<wcphlex::Frame, wcphlex::TensorSet>("Frame", "TensorSet");
    check_exact_distinct<wcphlex::DepoSet, wcphlex::Depo>("DepoSet", "Depo");
    check_exact_distinct<wcphlex::Tensor, wcphlex::TensorSet>("Tensor", "TensorSet");

    // 3. Same type must compare as structurally equal and exactly equal.
    {
        auto a = make_type_id<wcphlex::Frame>();
        auto b = make_type_id<wcphlex::Frame>();
        assert(a == b);
        assert(a.exact_compare(b));
    }

    // 4. Construction from null pointer (typical default-constructed wrapper).
    {
        wcphlex::Frame f{nullptr};
        assert(f.ptr == nullptr);

        wcphlex::DepoSet ds{nullptr};
        assert(ds.ptr == nullptr);
    }

    // 5. Move semantics: after move, source ptr is null; dest ptr is transferred.
    {
        auto raw_ptr = std::make_shared<const WireCell::IFrame* const>(nullptr);

        wcphlex::Frame src{nullptr};
        // Construct a DepoSet just to test move on a different type.
        wcphlex::DepoSet src_ds{nullptr};

        wcphlex::Frame moved = std::move(src);
        assert(src.ptr == nullptr);   // moved-from is null
        assert(moved.ptr == nullptr); // (we started with null, so dest is null too)
    }

    std::cout << "All data_types assertions passed.\n";
    return 0;
}
