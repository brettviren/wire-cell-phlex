/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wire_cell_phlex/FacadeWireSchema.hpp"

#include "WireCellUtil/Exceptions.h"
#include "WireCellUtil/Logging.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Units.h"

#include <cmath>

WIRECELL_FACTORY(FacadeWireSchema, wcphlex::FacadeWireSchema, WireCell::IWireSchema, WireCell::IConfigurable)
WIRECELL_FACTORY(FacadeDriftPlanes, wcphlex::FacadeDriftPlanes, WireCell::IDriftPlanes, WireCell::IConfigurable)

using namespace WireCell;

namespace wcphlex {

    std::mutex GeoRegistry::s_mutex;
    std::map<std::string, GeoRegistry::Entry> GeoRegistry::s_entries;

    void GeoRegistry::put(const std::string& key, const WireSchema& ws, const DriftPlanes& df)
    {
        if (!ws.store.db() || !df.planes) {
            raise<ValueError>("GeoRegistry: null geometry product for key \"%s\"", key);
        }
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_entries.find(key);
        if (it == s_entries.end()) {
            s_entries[key] = Entry{ws.store, df.planes};
            return;
        }
        if (it->second.store.db() != ws.store.db() || it->second.drift != df.planes) {
            raise<ValueError>("GeoRegistry: key \"%s\" already holds different geometry; "
                              "WCT can not follow a geometry change",
                              key);
        }
    }

    GeoRegistry::Entry GeoRegistry::get(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_entries.find(key);
        if (it == s_entries.end()) {
            raise<KeyError>("GeoRegistry: no geometry registered under key \"%s\" "
                            "(is the executor's geometry_key set?)",
                            key);
        }
        return it->second;
    }

    // ---- FacadeWireSchema ----

    FacadeWireSchema::FacadeWireSchema() = default;
    FacadeWireSchema::~FacadeWireSchema() = default;

    const WireCell::WireSchema::Store& FacadeWireSchema::wire_schema_store() const { return m_store; }

    void FacadeWireSchema::configure(Configuration const& cfg)
    {
        m_store = GeoRegistry::get(get<std::string>(cfg, "key", "geometry")).store;
    }

    Configuration FacadeWireSchema::default_configuration() const
    {
        Configuration cfg;
        cfg["key"] = "geometry";
        return cfg;
    }

    // ---- FacadeDriftPlanes ----

    FacadeDriftPlanes::FacadeDriftPlanes() = default;
    FacadeDriftPlanes::~FacadeDriftPlanes() = default;

    Configuration FacadeDriftPlanes::default_configuration() const
    {
        Configuration cfg;
        cfg["key"] = "geometry";
        // Distance from the collection plane to the response plane, toward
        // the cathode (the field response "origin").
        cfg["response_distance"] = 10 * units::cm;
        // Anode bound: collection plane moved this far into the drift.
        cfg["anode_offset"] = 0.0;
        // Faces whose planes agree within this make one xregion.
        cfg["tolerance"] = 0.1 * units::mm;
        return cfg;
    }

    void FacadeDriftPlanes::configure(Configuration const& cfg)
    {
        const auto key = get<std::string>(cfg, "key", "geometry");
        const double response_distance = get<double>(cfg, "response_distance", 10 * units::cm);
        const double anode_offset = get<double>(cfg, "anode_offset", 0.0);
        const double tolerance = get<double>(cfg, "tolerance", 0.1 * units::mm);

        const auto entry = GeoRegistry::get(key);
        std::map<std::pair<int, int>, gdmlwires::DriftPlane> by_ident;
        for (const auto& fd : *entry.drift) {
            by_ident[{fd.anode, fd.face}] = fd;
        }

        m_faces.clear();
        m_xregions = Json::arrayValue;
        std::vector<Configuration> distinct;
        for (const auto& anode : entry.store.anodes()) {
            Configuration jfaces = Json::arrayValue;
            for (const auto& face : entry.store.faces(anode)) {
                auto it = by_ident.find({anode.ident, face.ident});
                if (it == by_ident.end()) {
                    raise<KeyError>("FacadeDriftPlanes: no drift planes for anode %d face %d", anode.ident,
                                    face.ident);
                }
                const auto& fd = it->second;
                const double xanode = fd.collection - fd.drift_sign * anode_offset;
                const double response = fd.collection - fd.drift_sign * response_distance;
                if (fd.drift_sign * (xanode - response) <= 0) {
                    raise<ValueError>("anode %d: anode bound x=%f mm is not on the wire side of the response "
                                      "plane x=%f mm",
                                      anode.ident, xanode, response);
                }
                if (fd.drift_sign * (response - fd.cathode) <= 0) {
                    raise<ValueError>("anode %d: response plane x=%f mm is not between the cathode x=%f mm "
                                      "and the wires",
                                      anode.ident, response, fd.cathode);
                }
                Configuration jface;
                jface["anode"] = xanode;
                jface["response"] = response;
                jface["cathode"] = fd.cathode;
                jfaces.append(jface);

                bool seen = false;
                for (const auto& d : distinct) {
                    if (std::abs(d["anode"].asDouble() - xanode) < tolerance &&
                        std::abs(d["response"].asDouble() - response) < tolerance &&
                        std::abs(d["cathode"].asDouble() - fd.cathode) < tolerance) {
                        seen = true;
                        break;
                    }
                }
                if (!seen) {
                    distinct.push_back(jface);
                    m_xregions.append(jface);
                }
            }
            m_faces[anode.ident] = jfaces;
        }
        auto log = Log::logger("wcphlex");
        log->debug("FacadeDriftPlanes: {} anodes, {} xregions, response distance {} mm", m_faces.size(),
                   m_xregions.size(), response_distance);
    }

    Configuration FacadeDriftPlanes::faces(int anode_ident) const
    {
        auto it = m_faces.find(anode_ident);
        if (it == m_faces.end()) {
            raise<KeyError>("FacadeDriftPlanes: no faces for anode %d", anode_ident);
        }
        return it->second;
    }

    Configuration FacadeDriftPlanes::xregions() const { return m_xregions; }

}  // namespace wcphlex
