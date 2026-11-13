/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// drift_planes_check PDHD_GDML
//
// FacadeDriftPlanes on ProtoDUNE-HD GDML (readout/sorter "apa"): the outer
// (cryostat wall side) TPCs end ~2.5 cm from their collection planes, inside
// the 10 cm response distance.  short_drift "error" refuses them; "mirror"
// gives each the drift length of its APA's inner face.  Exit 0 if both hold.

#include "wire_cell_phlex/FacadeWireSchema.hpp"
#include "wire_cell_phlex/GdmlWires.hpp"

#include "gdml/objects.hpp"

#include "WireCellUtil/Exceptions.h"
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
    wcphlex::WireSchema ws{wcphlex::gdmlwires::wire_schema(doc, opt)};
    wcphlex::DriftPlanes dp{
        std::make_shared<const wcphlex::gdmlwires::DriftPlanes>(wcphlex::gdmlwires::drift_planes(doc, opt))};
    wcphlex::GeoRegistry::put("pdhd", ws, dp);

    Configuration cfg;
    cfg["key"] = "pdhd";
    cfg["response_distance"] = 10 * units::cm;
    cfg["anode_offset"] = 9.52 * units::mm;

    int nbad = 0;
    {
        wcphlex::FacadeDriftPlanes fdp;
        cfg["short_drift"] = "error";
        bool threw = false;
        try {
            fdp.configure(cfg);
        }
        catch (const ValueError&) {
            threw = true;
        }
        std::printf("short_drift error: %s\n", threw ? "refused (ok)" : "accepted (BAD)");
        nbad += !threw;
    }
    {
        wcphlex::FacadeDriftPlanes fdp;
        cfg["short_drift"] = "mirror";
        fdp.configure(cfg);
        for (int anode = 0; anode < 4; ++anode) {
            const auto faces = fdp.faces(anode);
            if (faces.size() != 2) {
                std::printf("anode %d: %d faces (BAD)\n", anode, (int) faces.size());
                ++nbad;
                continue;
            }
            double len[2];
            for (int f = 0; f < 2; ++f) {
                const double a = faces[f]["anode"].asDouble(), r = faces[f]["response"].asDouble(),
                             c = faces[f]["cathode"].asDouble();
                len[f] = std::abs(a - c);
                std::printf("anode %d face %d: anode %.2f response %.2f cathode %.2f mm\n", anode, f, a, r, c);
                // response between anode and cathode, 10 cm - anode_offset from the anode bound
                if (!((a - r) * (r - c) > 0) || std::abs(std::abs(a - r) - (100 - 9.52)) > 0.01) ++nbad;
            }
            if (std::abs(len[0] - len[1]) > 0.01 || len[0] < 3000) {
                std::printf("anode %d: face drift lengths %.2f, %.2f mm (BAD)\n", anode, len[0], len[1]);
                ++nbad;
            }
        }
    }
    std::printf("%s\n", nbad ? "FAIL" : "PASS");
    return nbad ? 1 : 0;
}
