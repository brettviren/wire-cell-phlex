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

// test/test_trivial.cpp
//
// Step 1 smoke test: verifies the package builds and links against
// wire_cell_phlex without error.  No logic beyond return 0.

#include "wire_cell_phlex/Executor.h"

int main()
{
  // Instantiating the base class confirms the header and shared library
  // are reachable and that the vtable links correctly.
  wcphlex::Executor e;
  (void)e;
  return 0;
}
