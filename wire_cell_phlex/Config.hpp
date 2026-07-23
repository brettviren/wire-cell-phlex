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

// wire_cell_phlex/Config.hpp
//
// Configuration schemas for the Executor node hierarchy, described as plain
// C++ aggregates using boost-config's Documented<T> for field docstrings.
//
// Boost.PFR (which boost-config uses) does NOT support inheritance, so the
// config schemas compose rather than inherit: every Executor subclass has its
// own *Config aggregate that carries a mandatory `executor` field of type
// ExecutorConfig.  This keeps the JSON shape uniform across all nodes
// (always { "executor": {...}, ... }) — a field-less node can gain its own
// fields later as a purely additive change — and lets each node emit a
// complete, self-contained Jsonnet factory (the ExecutorConfig sub-schema is
// pulled in by recursion).
//
// This header is intentionally lightweight: it only defines the aggregates and
// pulls in Documented.hpp (pure C++20, no PFR).  The boost::json (de)serial-
// ization hooks live in Config_json.hpp, which is included only by the .cpp
// files that actually parse configuration (Executor.cpp) — keeping the heavy
// PFR machinery, and its compiler requirements, out of this public header.

#include "boost_config/Documented.hpp"

#include <map>
#include <string>
#include <vector>

namespace wcphlex {

using boost_config::Documented;

// Common WCT-executor configuration consumed by every shape executor (via
// register_shapes.hpp's executor_config_from / Config_json.hpp value_to).
struct ExecutorConfig {
    Documented<std::string> wct_config{
        "", "Path to the WCT Jsonnet config file (required). A Jsonnet function "
            "accepting at least the source_name/sink_name/app_name TLAs."};
    Documented<std::string> wct_app{
        "Pgrapher", "WCT IApplication type used to build the add_app() name."};
    Documented<std::vector<std::string>> wct_plugins{
        {}, "Additional WCT plugin libraries to load (e.g. WireCellPgraph)."};
    Documented<std::map<std::string, std::string>> wct_tla{
        {}, "Extra Jsonnet top-level arguments (string -> string)."};
    Documented<std::string> module_label{
        "", "Scope prefix for WCT component instance names. Empty selects the "
            "default scope 'wcphlex'."};
    Documented<std::string> wct_log_sink{
        "", "WCT log destination: 'stdout', 'stderr', or a file path. Empty = none."};
    Documented<std::string> wct_log_level{
        "", "WCT log level ('warn', 'info', 'debug', ...). Only used with wct_log_sink."};
};

} // namespace wcphlex
