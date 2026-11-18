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
#include <cstdlib>

using namespace WireCell;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::printf("usage: drift_planes_check APA_GDML [NANODES NINSENSITIVE NXREGIONS]\n"
                    "  defaults: ProtoDUNE-HD, 4 APAs, 4 insensitive faces, 2 xregions\n");
        return 2;
    }
    const int want_anodes = argc > 2 ? std::atoi(argv[2]) : 4;
    const int want_insensitive = argc > 3 ? std::atoi(argv[3]) : 4;
    const int want_xregions = argc > 4 ? std::atoi(argv[4]) : 2;
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
    nbad += ninsensitive != want_insensitive;

    wcphlex::WireSchema ws{wcphlex::gdmlwires::wire_schema(doc, opt)};
    wcphlex::DriftPlanes dp{std::make_shared<const wcphlex::gdmlwires::DriftPlanes>(planes)};
    wcphlex::GeoRegistry::put("apa", ws, dp);

    Configuration cfg;
    cfg["key"] = "apa";
    cfg["response_distance"] = 10 * units::cm;
    cfg["anode_offset"] = 9.52 * units::mm;
    wcphlex::FacadeDriftPlanes fdp;
    fdp.configure(cfg);
    int nnull_total = 0;
    for (int anode = 0; anode < want_anodes; ++anode) {
        const auto faces = fdp.faces(anode);
        int nnull = 0, nfull = 0;
        for (const auto& jf : faces) {
            if (jf.isNull()) {
                ++nnull;
                if (want_anodes <= 4) std::printf("anode %d: null face\n", anode);
                continue;
            }
            const double a = jf["anode"].asDouble(), r = jf["response"].asDouble(), c = jf["cathode"].asDouble();
            if (want_anodes <= 4) std::printf("anode %d: anode %.2f response %.2f cathode %.2f mm\n", anode, a, r, c);
            if ((a - r) * (r - c) > 0 && std::abs(a - c) > 3000) ++nfull;
        }
        nnull_total += nnull;
        // Every face is either null (insensitive) or a full drift.
        if (faces.size() != 2 || nnull + nfull != 2) {
            std::printf("anode %d: want two faces, each null or a full drift (BAD)\n", anode);
            ++nbad;
        }
    }
    std::printf("%d null faces\n", nnull_total);
    nbad += nnull_total != want_insensitive;
    const auto xr = fdp.xregions();
    std::printf("%d xregions\n", (int) xr.size());
    // Distinct sensitive drift regions: PDHD the inner faces at -x and +x
    // (APAs 0,2 and 1,3 coincide); full FD-HD the four between its CPAs.
    nbad += (int) xr.size() != want_xregions;
    std::printf("%s\n", nbad ? "FAIL" : "PASS");
    return nbad ? 1 : 0;
}
