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

// wire_cell_phlex/FacadeWireSchema.h
//
// FacadeWireSchema: WCT IWireSchema + IConfigurable component that draws its
// data from a static registry pre-populated by the Executor before
// WireCell::Main::initialize() is called.
//
// MOTIVATION:
//   WCT's configure-time service pattern: AnodePlane::configure() calls
//   Factory::find_tn<IWireSchema>(name) and extracts all geometry immediately.
//   In the PHLEX integration, geometry arrives as a PHLEX job-layer product
//   (wcphlex::WireSchema) — after the WCT configure phase would normally have
//   run.  The static registry decouples the two timelines:
//
//     1. PHLEX invokes operator()(wcphlex::WireSchema const&, wcphlex::Frame const&)
//        on the first event.
//     2. The executor calls FacadeWireSchema::register_store(scope, store) to
//        deposit the WireSchema::Store in the static map under the executor's scope.
//     3. The executor calls m_wcmain.initialize() — during configure, the
//        FacadeWireSchema instance reads the store from the static map by scope.
//     4. AnodePlane::configure() finds the FacadeWireSchema instance in the factory
//        (just created by initialize()) and extracts geometry as usual.
//
// STATIC REGISTRY:
//   s_stores: std::map<std::string, WireSchema::Store>
//   s_mutex:  protects concurrent access (multiple executor instances)
//   register_store(scope, store): called by Executor before initialize()
//   configure(cfg): reads "scope" key, copies store into m_store
//
// WireSchemaValidator:
//   A lightweight IConfigurable helper that validates the IWireSchema service
//   can be found in the factory and contains at least one wire.  Intended for
//   use at the end of a WCT graph config to confirm geometry is accessible.

#include "WireCellUtil/IComponent.h"
#include "WireCellUtil/WireSchema.h"
#include "WireCellIface/IWireSchema.h"
#include "WireCellIface/IConfigurable.h"

#include <map>
#include <mutex>
#include <string>

namespace wcphlex {

class FacadeWireSchema : public WireCell::IWireSchema,
                         public WireCell::IConfigurable {
public:
    FacadeWireSchema();
    virtual ~FacadeWireSchema();

    // Pre-populate the static registry BEFORE calling WireCell::Main::initialize().
    // scope_name must match the "scope" key in the WCT Jsonnet config for this
    // FacadeWireSchema instance.
    static void register_store(std::string const& scope_name,
                               WireCell::WireSchema::Store store);

    // IWireSchema
    const WireCell::WireSchema::Store& wire_schema_store() const override;

    // IConfigurable
    void configure(WireCell::Configuration const& cfg) override;
    WireCell::Configuration default_configuration() const override;

private:
    std::string                  m_scope;
    WireCell::WireSchema::Store  m_store;

    static std::mutex                                      s_mutex;
    static std::map<std::string, WireCell::WireSchema::Store> s_stores;
};

// ---------------------------------------------------------------------------
// WireSchemaValidator
//
// WCT IConfigurable test helper: looks up a named IWireSchema service in the
// WCT factory and asserts it has at least one wire.  Useful as the last
// component in a test WCT graph to confirm geometry is reachable.
//
// Expected config keys:
//   wire_schema (string, required): "Type:name" tag of the IWireSchema service.
// ---------------------------------------------------------------------------
class WireSchemaValidator : public WireCell::IConfigurable {
public:
    virtual ~WireSchemaValidator();

    void configure(WireCell::Configuration const& cfg) override;
    WireCell::Configuration default_configuration() const override;
};

} // namespace wcphlex
