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

// wire_cell_phlex/Config_json.hpp
//
// boost::json (de)serialization hooks for the config schemas in Config.hpp.
//
// Separated from Config.hpp because these instantiate boost-config's PFR-based
// machinery (boost::pfr::get_name etc.), which is heavier and has stronger
// compiler requirements than the plain aggregate definitions.  Include this
// ONLY in translation units that actually parse configuration (i.e. those that
// call boost::json::value_to<...Config>()), such as Executor.cpp.  The public
// Executor.hpp deliberately includes only Config.hpp.

#include "wire_cell_phlex/Config.hpp"

#include "boost_config/json.hpp"

namespace wcphlex {

BOOST_CONFIG_JSON(ExecutorConfig)

} // namespace wcphlex
