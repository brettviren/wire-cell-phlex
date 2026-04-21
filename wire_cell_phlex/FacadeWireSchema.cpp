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

// wire_cell_phlex/FacadeWireSchema.cpp
//
// Implements FacadeWireSchema and WireSchemaValidator.
//
// FacadeWireSchema bridges two timelines:
//   - PHLEX time: wcphlex::WireSchema arrives as a job-layer product.
//   - WCT time:   AnodePlane::configure() fetches IWireSchema during initialize().
//
// The Executor calls register_store(scope, store) before initialize().
// During configure(), this instance copies the store from the static map.

#include "wire_cell_phlex/FacadeWireSchema.h"

#include <WireCellUtil/NamedFactory.h>
#include <WireCellUtil/Exceptions.h>

#include <stdexcept>

// Register FacadeWireSchema as a WCT factory type implementing IWireSchema
// and IConfigurable.  The WIRECELL_FACTORY macro creates:
//   extern "C" make_FacadeWireSchema_factory()  (dlopen symbol)
//   static hello_FacadeWireSchema_me             (self-registrar)
WIRECELL_FACTORY(FacadeWireSchema,
                 wcphlex::FacadeWireSchema,
                 WireCell::IWireSchema,
                 WireCell::IConfigurable)

// Register WireSchemaValidator as a WCT IConfigurable only.
WIRECELL_FACTORY(WireSchemaValidator,
                 wcphlex::WireSchemaValidator,
                 WireCell::IConfigurable)

namespace wcphlex {

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------
std::mutex                                         FacadeWireSchema::s_mutex;
std::map<std::string, WireCell::WireSchema::Store> FacadeWireSchema::s_stores;

// ---------------------------------------------------------------------------
// FacadeWireSchema
// ---------------------------------------------------------------------------

FacadeWireSchema::FacadeWireSchema() = default;
FacadeWireSchema::~FacadeWireSchema() = default;

void FacadeWireSchema::register_store(std::string const& scope_name,
                                      WireCell::WireSchema::Store store)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    s_stores[scope_name] = std::move(store);
}

const WireCell::WireSchema::Store& FacadeWireSchema::wire_schema_store() const
{
    return m_store;
}

void FacadeWireSchema::configure(WireCell::Configuration const& cfg)
{
    m_scope = cfg["scope"].asString();

    std::lock_guard<std::mutex> guard(s_mutex);
    auto it = s_stores.find(m_scope);
    if (it == s_stores.end()) {
        throw std::runtime_error(
            "FacadeWireSchema::configure: no store registered for scope \"" +
            m_scope + "\" — call register_store() before initialize()");
    }
    m_store = it->second;
}

WireCell::Configuration FacadeWireSchema::default_configuration() const
{
    WireCell::Configuration cfg;
    cfg["scope"] = "";  // must be set to the executor's scope name
    return cfg;
}

// ---------------------------------------------------------------------------
// WireSchemaValidator
// ---------------------------------------------------------------------------

WireSchemaValidator::~WireSchemaValidator() = default;

void WireSchemaValidator::configure(WireCell::Configuration const& cfg)
{
    auto ws_tn = cfg["wire_schema"].asString();
    if (ws_tn.empty()) {
        throw std::runtime_error("WireSchemaValidator: 'wire_schema' config key is required");
    }

    // Factory::find_tn splits "Type:name" and calls find<Iface>(type, name).
    auto ws = WireCell::Factory::find_tn<WireCell::IWireSchema>(ws_tn);
    if (!ws) {
        throw std::runtime_error(
            "WireSchemaValidator: cannot find IWireSchema \"" + ws_tn + "\" in WCT factory");
    }
    if (ws->wire_schema_store().wires().empty()) {
        throw std::runtime_error(
            "WireSchemaValidator: IWireSchema \"" + ws_tn + "\" has no wires");
    }
}

WireCell::Configuration WireSchemaValidator::default_configuration() const
{
    WireCell::Configuration cfg;
    cfg["wire_schema"] = "";  // e.g. "FacadeWireSchema:myScope"
    return cfg;
}

} // namespace wcphlex
