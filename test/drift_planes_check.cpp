/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// drift_planes_check PDHD_GDML
//
// Insensitive faces from GDML: in ProtoDUNE-HD GDML (readout/sorter "apa")
// the outer (cryostat wall side) TPC of each APA is ~2.5 cm deep against the
// inner TPCs' ~3.5 m, so gdmlwires marks it insensitive and FacadeDriftPlanes
// gives a null face for it (as WCT's AnodePlane takes it) and no xregion,
// while each inner face has anode/response/cathode with the cathode ~3.5 m
// away.  Exit 0 if so.

#include "wire_cell_phlex/FacadeWireSchema.hpp"
#include "wire_cell_phlex/GdmlWires.hpp"

#include "gdml/objects.hpp"

#include "WireCellUtil/Units.h"

#include <cmath>
#include <cstdio>

using namespace WireCell;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::printf("usage: drift_planes_check PDHD_GDML\n");
        return 2;
    }
    auto doc = gdml::read(argv[1]);
    wcphlex::gdmlwires::Options opt;
    opt.readout = "apa";
    opt.sorter = "apa";
    const auto planes = wcphlex::gdmlwires::drift_planes(doc, opt);
    int nbad = 0, ninsensitive = 0;
    for (const auto& dp : planes) {
        ninsensitive += !dp.sensitive;
    }
    std::printf("%zu faces, %d insensitive\n", planes.size(), ninsensitive);
    nbad += ninsensitive != 4;

    wcphlex::WireSchema ws{wcphlex::gdmlwires::wire_schema(doc, opt)};
    wcphlex::DriftPlanes dp{std::make_shared<const wcphlex::gdmlwires::DriftPlanes>(planes)};
    wcphlex::GeoRegistry::put("pdhd", ws, dp);

    Configuration cfg;
    cfg["key"] = "pdhd";
    cfg["response_distance"] = 10 * units::cm;
    cfg["anode_offset"] = 9.52 * units::mm;
    wcphlex::FacadeDriftPlanes fdp;
    fdp.configure(cfg);
    for (int anode = 0; anode < 4; ++anode) {
        const auto faces = fdp.faces(anode);
        int nnull = 0, nfull = 0;
        for (const auto& jf : faces) {
            if (jf.isNull()) {
                ++nnull;
                std::printf("anode %d: null face\n", anode);
                continue;
            }
            const double a = jf["anode"].asDouble(), r = jf["response"].asDouble(), c = jf["cathode"].asDouble();
            std::printf("anode %d: anode %.2f response %.2f cathode %.2f mm\n", anode, a, r, c);
            if ((a - r) * (r - c) > 0 && std::abs(a - c) > 3000) ++nfull;
        }
        if (faces.size() != 2 || nnull != 1 || nfull != 1) {
            std::printf("anode %d: want one null and one full face (BAD)\n", anode);
            ++nbad;
        }
    }
    const auto xr = fdp.xregions();
    std::printf("%d xregions\n", (int) xr.size());
    nbad += xr.size() != 2;  // the inner faces at -x and +x (APAs 0,2 and 1,3 coincide)
    std::printf("%s\n", nbad ? "FAIL" : "PASS");
    return nbad ? 1 : 0;
}
