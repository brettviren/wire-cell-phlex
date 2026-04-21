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
// Helper: convert phlex::configuration → boost::json::object for Executor.
//
// Executor (and its subclasses) accept boost::json::object rather than
// phlex::configuration, because Executor.h must remain compilable under
// GCC 12 and phlex/configuration.hpp requires GCC 13+ (std::forward_like).
// PHLEX MODULE files are compiled with GCC 15, so they can include this
// header which uses phlex::configuration freely.

#include "phlex/configuration.hpp"

#include <boost/json.hpp>

inline boost::json::object to_executor_config(phlex::configuration const& cfg)
{
    boost::json::object obj;

    obj["wct_config"] = cfg.get<std::string>("wct_config");

    if (auto v = cfg.get_if_present<std::string>("wct_app")) {
        obj["wct_app"] = *v;
    }

    if (auto v = cfg.get_if_present<std::vector<std::string>>("wct_plugins")) {
        boost::json::array arr;
        for (auto const& s : *v) { arr.push_back(boost::json::value{boost::json::string{s}}); }
        obj["wct_plugins"] = std::move(arr);
    }

    // Pass module_label through so the Executor can derive unique WCT
    // component instance names for each PHLEX module instance.
    if (auto v = cfg.get_if_present<std::string>("module_label")) {
        obj["module_label"] = *v;
    }

    if (auto tla = cfg.get_if_present<phlex::configuration>("wct_tla")) {
        boost::json::object tla_obj;
        for (auto const& k : tla->keys()) {
            tla_obj[k] = tla->get<std::string>(k);
        }
        obj["wct_tla"] = std::move(tla_obj);
    }

    return obj;
}
