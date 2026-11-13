/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// wire_cell_phlex/GdmlWires.hpp
//
// From LArSoft-convention GDML with exhaustive wire placements to WCT wire
// and drift-plane data, without LArSoft.  This is a deliberately monolithic
// port of what LArSoft + larwirecell compute for the same GDML:
//
// - geometry: TPCs are volumes named volTPC* (not volTPCActive/Plane/Wire),
//   each with a volTPCActive daughter, volTPCPlane* planes and volTPCWire*
//   tube wires (larcorealg GeometryBuilderStandard).  Planes are ordered by
//   increasing distance from the TPC center along the drift (WireReadoutGeom
//   ::SortPlanes); wire endpoints are the tube center -+ half length along
//   its axis.
// - readout "crp": dunecore CRPWireReadoutGeom with GeoObjectSorterCRU60D
//   (TPCs by drift coordinate, then Z, then Y), TPC sets of 2x2 TPCs,
//   CRPWireReadoutSorter wire order, and induction channels shared by the
//   two TPCs of a pair (jumpered CRU), found by NearestWireID continuation.
// - grouping "pair-tpc": WCT anode = gtpc/2, face = (1+gtpc)%2, where gtpc is
//   the sorted TPC number (larwirecell WireSchemaBuilder kPairTPC).
// - store: one wire per (channel, segment) in channel order, segment = the
//   wire's position in the channel's ROP plane list, then
//   WireSchema::correct() to the requested level.
// - drift planes: per TPC the collection-plane X, cathode X (active-volume face
//   opposite the planes) and drift sign (larwirecell build_drift_planes).
//
// Only drift along X and one cryostat are supported, as in
// CRPWireReadoutGeom.

#include "WireCellUtil/WireSchema.h"

#include <map>
#include <string>
#include <vector>

namespace gdml {
    class Document;
}

namespace wcphlex::gdmlwires {

    struct Options {
        // Channel map: "crp" (dunecore CRPWireReadoutGeom: FD-VD, ProtoDUNE-VD)
        // or "apa" (dunecore DuneApaWireReadoutGeom with WireReadoutSorterAPA,
        // as ProtoDUNEWireReadoutGeom: ProtoDUNE-HD).
        std::string readout{"crp"};
        // TPC sorter: "cru60d" (GeoObjectSorterCRU60D, FD-VD), "cru60d-pdvd"
        // (the same with SortTPCPDVD: true, ProtoDUNE-VD) or "apa"
        // (GeoObjectSorterAPA, ProtoDUNE-HD).
        std::string sorter{"cru60d"};
        std::string grouping{"pair-tpc"};  // only "pair-tpc" for now
        // WireSchema correction level applied to the store (4 = pitch, as
        // wcwires and larwirecell's WireSchemaFileWriter use).
        int correction{4};
        // A TPC whose drift length (collection plane to cathode) is less
        // than this fraction of the longest one in the detector is an
        // insensitive face: eg the ~2.5 cm between a ProtoDUNE-HD APA and
        // the cryostat wall (3.5 m drifts).
        double min_drift_fraction{0.1};
    };

    /// Drift geometry of one face (one LArSoft TPC), WCT units (mm).
    struct DriftPlane {
        int anode{0}, face{0};   // WCT idents
        double collection{0};    // collection-plane X
        double cathode{0};       // cathode X
        int drift_sign{0};       // +1: electrons drift toward +X
        // False for a face with no real drift volume (Options
        // min_drift_fraction): WCT gives it a null face, no sensitive volume.
        bool sensitive{true};
    };
    using DriftPlanes = std::vector<DriftPlane>;  // one per TPC, sorted by (anode, face)

    /// Build the corrected wire store.  Throws WireCell::ValueError on
    /// geometry it cannot interpret.
    WireCell::WireSchema::Store wire_schema(const gdml::Document& doc, const Options& opt = {});

    /// Build the drift planes.
    DriftPlanes drift_planes(const gdml::Document& doc, const Options& opt = {});

}  // namespace wcphlex::gdmlwires
