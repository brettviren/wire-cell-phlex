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

// test/test_shape_executors.cpp  (Idea-2, ddm-7dk.3/.4)
//
// Compile-only proof for the shape-executor family: every shape instantiates
// (types + operator() signatures) and its operator() body is forced to
// instantiate (via a member-function-pointer odr-use), catching template errors
// in run()/fill/drain/find without needing a live WCT graph per shape.

#include "wire_cell_phlex/ShapeExecutors.hpp"

#include <WireCellIface/IFrame.h>
#include <WireCellIface/IDepoSet.h>

#include <iostream>
#include <tuple>
#include <type_traits>

using namespace wcphlex;
using WireCell::IDepoSet;
using WireCell::IFrame;

// --- operator() signatures (instantiates declarations) ----------------------
static_assert(std::is_same_v<
    decltype(std::declval<FunctionExecutor<IFrame, IFrame>&>()(std::declval<Data<IFrame>>())),
    Data<IFrame>>);

static_assert(std::is_same_v<
    decltype(std::declval<SinkExecutor<IFrame>&>()(std::declval<Data<IFrame>>())),
    void>);

static_assert(std::is_same_v<
    decltype(std::declval<SourceExecutor<IFrame>&>()()),
    Data<IFrame>>);

static_assert(std::is_same_v<
    decltype(std::declval<FaninExecutor<IFrame, IFrame, 4>&>()(
        std::declval<Data<IFrame>>(), std::declval<Data<IFrame>>(),
        std::declval<Data<IFrame>>(), std::declval<Data<IFrame>>())),
    Data<IFrame>>);

static_assert(std::is_same_v<
    decltype(std::declval<FanoutExecutor<IFrame, IFrame, 3>&>()(std::declval<Data<IFrame>>())),
    std::tuple<Data<IFrame>, Data<IFrame>, Data<IFrame>>>);

// heterogeneous join: IFrame + IDepoSet -> IFrame
static_assert(std::is_same_v<
    decltype(std::declval<JoinExecutor<type_list<IFrame, IDepoSet>, IFrame>&>()(
        std::declval<Data<IFrame>>(), std::declval<Data<IDepoSet>>())),
    Data<IFrame>>);

// heterogeneous split: IFrame -> (IFrame, IDepoSet)
static_assert(std::is_same_v<
    decltype(std::declval<SplitExecutor<IFrame, type_list<IFrame, IDepoSet>>&>()(
        std::declval<Data<IFrame>>())),
    std::tuple<Data<IFrame>, Data<IDepoSet>>>);

// --- force operator() body instantiation ------------------------------------
template <class T>
void odr_use(T) {}

int main()
{
    odr_use(&FunctionExecutor<IFrame, IFrame>::operator());
    odr_use(&SinkExecutor<IFrame>::operator());
    odr_use(&SourceExecutor<IFrame>::operator());
    odr_use(&FaninExecutor<IFrame, IFrame, 4>::operator());
    odr_use(&FanoutExecutor<IFrame, IFrame, 3>::operator());
    odr_use(&JoinExecutor<type_list<IFrame, IDepoSet>, IFrame>::operator());
    odr_use(&SplitExecutor<IFrame, type_list<IFrame, IDepoSet>>::operator());

    std::cout << "shape executors compile + instantiate: PASS\n";
    return 0;
}
