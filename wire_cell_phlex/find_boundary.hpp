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

// wire_cell_phlex/find_boundary.hpp
//
// Look up a WCT component in the global NamedFactory by class + instance name
// and cast it to the concrete boundary type.  Shared ownership is preserved
// (dynamic_pointer_cast): the returned shared_ptr co-owns the instance with the
// factory registry, so callers can hold it without relying on the registry to
// keep it alive.

#include <WireCellUtil/NamedFactory.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace wcphlex {

template <typename Iface, typename Concrete>
std::shared_ptr<Concrete> find_boundary(std::string const& classname,
                                        std::string const& instname)
{
    auto iface = WireCell::Factory::find_maybe<Iface>(classname, instname);
    if (!iface) {
        throw std::runtime_error("find_boundary: WCT factory has no instance " +
                                 classname + ":" + instname +
                                 " — check that the Jsonnet config creates it with the correct name");
    }
    auto concrete = std::dynamic_pointer_cast<Concrete>(iface);
    if (!concrete) {
        throw std::runtime_error("find_boundary: dynamic_pointer_cast to concrete boundary type "
                                 "failed for " + classname + ":" + instname);
    }
    return concrete;
}

} // namespace wcphlex
