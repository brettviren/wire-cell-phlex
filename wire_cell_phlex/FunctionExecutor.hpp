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

// wire_cell_phlex/FunctionExecutor.hpp
//
// FunctionExecutor<In,Out> is the 1-in/1-out member of the shape-executor
// family.  It (and the whole family) now lives in ShapeExecutors.hpp; this
// header is kept as the entry point for the common single-transform case.

#include "wire_cell_phlex/ShapeExecutors.hpp"
