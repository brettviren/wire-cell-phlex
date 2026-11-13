/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wire_cell_phlex/FacadeWireSchema.hpp"

#include <algorithm>

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
        // A face whose drift volume ends before its response plane (eg the
        // few cm between a ProtoDUNE-HD APA and the cryostat wall):
        //   "error":  refuse the geometry;
        //   "mirror": give it the drift length of the other face of its
        //             anode (both faces active and symmetric, the usual
        //             simulation geometry of an APA detector).
        cfg["short_drift"] = "error";
        return cfg;
    }

    void FacadeDriftPlanes::configure(Configuration const& cfg)
    {
        const auto key = get<std::string>(cfg, "key", "geometry");
        const double response_distance = get<double>(cfg, "response_distance", 10 * units::cm);
        const double anode_offset = get<double>(cfg, "anode_offset", 0.0);
        const double tolerance = get<double>(cfg, "tolerance", 0.1 * units::mm);
        const auto short_drift = get<std::string>(cfg, "short_drift", "error");
        if (short_drift != "error" && short_drift != "mirror") {
            raise<ValueError>("FacadeDriftPlanes: short_drift must be \"error\" or \"mirror\", got \"%s\"",
                              short_drift);
        }

        const auto entry = GeoRegistry::get(key);
        std::map<std::pair<int, int>, gdmlwires::DriftPlane> by_ident;
        for (const auto& fd : *entry.drift) {
            by_ident[{fd.anode, fd.face}] = fd;
        }

        m_faces.clear();
        m_xregions = Json::arrayValue;
        std::vector<Configuration> distinct;
        // Drift length from the collection plane to the cathode.
        auto drift_length = [](const gdmlwires::DriftPlane& fd) { return fd.drift_sign * (fd.collection - fd.cathode); };
        for (const auto& anode : entry.store.anodes()) {
            Configuration jfaces = Json::arrayValue;
            for (const auto& face : entry.store.faces(anode)) {
                auto it = by_ident.find({anode.ident, face.ident});
                if (it == by_ident.end()) {
                    raise<KeyError>("FacadeDriftPlanes: no drift planes for anode %d face %d", anode.ident,
                                    face.ident);
                }
                auto fd = it->second;
                if (short_drift == "mirror" && drift_length(fd) <= response_distance) {
                    // The longest drift among the anode's other faces.
                    double length = 0;
                    for (const auto& other : entry.store.faces(anode)) {
                        if (other.ident == face.ident) continue;
                        auto ot = by_ident.find({anode.ident, other.ident});
                        if (ot != by_ident.end()) length = std::max(length, drift_length(ot->second));
                    }
                    if (length <= response_distance) {
                        raise<ValueError>("anode %d face %d: drift volume ends before the response plane and "
                                          "no other face of the anode has a longer one",
                                          anode.ident, face.ident);
                    }
                    fd.cathode = fd.collection - fd.drift_sign * length;
                }
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
