/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// wire_cell_phlex/FacadeWireSchema.hpp
//
// WCT components that take their geometry from Phlex job-layer products.
//
// WCT components are made by WCT's factory and configured from Jsonnet; they
// cannot be handed a Phlex product.  So an executor whose module declares
// geometry inputs (ExecutorConfig::geometry_key) puts the products it receives
// in a process-wide GeoRegistry under that key, on its first call, and only
// then configures WCT (WireCell::Main::init_configure).  These components read
// the registry in their configure():
//
//   FacadeWireSchema  (IWireSchema)  {key}: the wire store.
//   FacadeDriftPlanes (IDriftPlanes) {key, response_distance, anode_offset,
//                                     tolerance}: per anode, per face (in the
//                     store's face order) the anode/response/cathode X, as
//                     larwirecell's DriftPlanesSource computes them.
//
// All executors in a job register the same products under the same key, so
// the components' configurations are identical and WCT shares one instance
// (a later Main skips an identically configured component).

#include "wire_cell_phlex/Data.hpp"

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/IDriftPlanes.h"
#include "WireCellIface/IWireSchema.h"
#include "WireCellUtil/WireSchema.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace wcphlex {

    class GeoRegistry {
       public:
        struct Entry {
            WireCell::WireSchema::Store store;
            std::shared_ptr<const gdmlwires::DriftPlanes> drift;
        };

        /// Register products under key.  Registering again with the same
        /// objects is a no-op; with different ones it throws (WCT cannot
        /// follow a geometry change).
        static void put(const std::string& key, const WireSchema& ws, const DriftPlanes& df);

        /// The entry for key; throws if absent.
        static Entry get(const std::string& key);

       private:
        static std::mutex s_mutex;
        static std::map<std::string, Entry> s_entries;
    };

    class FacadeWireSchema : public WireCell::IWireSchema, public WireCell::IConfigurable {
       public:
        FacadeWireSchema();
        virtual ~FacadeWireSchema();

        const WireCell::WireSchema::Store& wire_schema_store() const override;

        void configure(WireCell::Configuration const& cfg) override;
        WireCell::Configuration default_configuration() const override;

       private:
        WireCell::WireSchema::Store m_store;
    };

    class FacadeDriftPlanes : public WireCell::IDriftPlanes, public WireCell::IConfigurable {
       public:
        FacadeDriftPlanes();
        virtual ~FacadeDriftPlanes();

        WireCell::Configuration faces(int anode_ident) const override;
        WireCell::Configuration xregions() const override;

        void configure(WireCell::Configuration const& cfg) override;
        WireCell::Configuration default_configuration() const override;

       private:
        std::map<int, WireCell::Configuration> m_faces;
        WireCell::Configuration m_xregions;
    };

}  // namespace wcphlex
