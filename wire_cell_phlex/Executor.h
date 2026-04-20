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
// wire_cell_phlex/Executor.h
//
// WctExecutor: manages a WireCell::Main instance for per-event use within a
// PHLEX concurrency::serial transform node.
//
// Full implementation arrives in Step 4.  This header is a placeholder that
// establishes the namespace and will be expanded incrementally.

namespace wcphlex {

  /// Base class for WCT sub-graph executors.
  /// Subclasses (FrameFilter, DepoSetToFrame, …) are added in Step 4.
  class Executor {
  public:
    Executor() = default;
    virtual ~Executor() = default;

    Executor(Executor const&) = delete;
    Executor& operator=(Executor const&) = delete;
  };

} // namespace wcphlex
