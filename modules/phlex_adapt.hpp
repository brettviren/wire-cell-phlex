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

// modules/phlex_adapt.hpp
//
// Bridge the generic phlex_config schema types to the concrete Phlex
// registration types used inside a PHLEX_REGISTER_* body.

#include "phlex_config/PhlexAlgorithmConfig.hpp"

#include "phlex/module.hpp"

#include <cstddef>
#include <string>

namespace wcphlex {

// "serial" | "unlimited" | an integer (as a string) -> phlex::concurrency.
inline phlex::concurrency to_concurrency(std::string const& s)
{
    if (s == "serial") {
        return phlex::concurrency::serial;
    }
    if (s == "unlimited") {
        return phlex::concurrency::unlimited;
    }
    return phlex::concurrency{static_cast<std::size_t>(std::stoul(s))};
}

// phlex_config::ProductSelector -> phlex::product_selector.  The product type is
// left unset; Phlex populates it from the callable's parameter type when the
// selector is handed to input_family().  Empty suffix/stage are left unset so
// they match any.
inline phlex::product_selector to_selector(phlex_config::ProductSelector const& ps)
{
    phlex::product_selector sel{
        .creator = std::string(ps.creator),
        .layer = std::string(ps.layer),
    };
    if (const std::string suffix = ps.suffix; !suffix.empty()) {
        sel.suffix = phlex::experimental::identifier{suffix};
    }
    if (const std::string stage = ps.stage; !stage.empty()) {
        sel.stage = phlex::experimental::identifier{stage};
    }
    return sel;
}

} // namespace wcphlex
