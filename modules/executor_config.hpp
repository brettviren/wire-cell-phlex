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

// modules/executor_config.h
//
// Helper: convert phlex::configuration → the boost::json::object an Executor
// node expects.
//
// Every Executor node's config is uniform in shape: a single "executor" key
// holding the ExecutorConfig sub-object (see wire_cell_phlex/Config.hpp).  This
// helper builds exactly that wrapper.  Nodes that add their own fields (e.g.
// FrameFilter's use_wire_schema) set them alongside "executor" in the module
// after calling this.
//
// Executor (and its subclasses) accept boost::json::object rather than
// phlex::configuration, because Executor.h must remain compilable under
// GCC 12 and phlex/configuration.hpp requires GCC 13+ (std::forward_like).
// PHLEX MODULE files are compiled with GCC 15, so they can include this
// header which uses phlex::configuration freely.

#include "phlex/configuration.hpp"

#include <boost/json.hpp>

// Build the { "executor": {...} } wrapper object for an Executor node.
inline boost::json::object to_executor_config(phlex::configuration const& cfg)
{
    boost::json::object ex;

    ex["wct_config"] = cfg.get<std::string>("wct_config");

    if (auto v = cfg.get_if_present<std::string>("wct_app")) {
        ex["wct_app"] = *v;
    }

    if (auto v = cfg.get_if_present<std::vector<std::string>>("wct_plugins")) {
        boost::json::array arr;
        for (auto const& s : *v) { arr.push_back(boost::json::value{boost::json::string{s}}); }
        ex["wct_plugins"] = std::move(arr);
    }

    // Pass module_label through so the Executor can derive unique WCT
    // component instance names for each PHLEX module instance.
    if (auto v = cfg.get_if_present<std::string>("module_label")) {
        ex["module_label"] = *v;
    }

    if (auto tla = cfg.get_if_present<phlex::configuration>("wct_tla")) {
        boost::json::object tla_obj;
        for (auto const& k : tla->keys()) {
            tla_obj[k] = tla->get<std::string>(k);
        }
        ex["wct_tla"] = std::move(tla_obj);
    }

    // Optional: WCT log sink ("stdout", "stderr", or a file path).
    if (auto v = cfg.get_if_present<std::string>("wct_log_sink")) {
        ex["wct_log_sink"] = *v;
    }

    // Optional: WCT log level ("warn", "info", "debug", etc.).
    if (auto v = cfg.get_if_present<std::string>("wct_log_level")) {
        ex["wct_log_level"] = *v;
    }

    boost::json::object obj;
    obj["executor"] = std::move(ex);
    return obj;
}
