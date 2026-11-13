/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * gdml_wires_check GDML [REFERENCE_WIRES]
 *
 * Build the WCT wire store and drift planes from LArSoft-convention GDML
 * (wire_cell_phlex/GdmlWires) and, given a reference wires file (e.g. one
 * written by larwirecell's WireSchemaFileWriter from the same GDML through
 * LArSoft), compare: anode/face/plane idents, and per plane, wire by wire in
 * order, the wire ident, channel, segment and endpoints.  Exit 1 on any
 * mismatch beyond 0.05 mm.
 */

#include "wire_cell_phlex/GdmlWires.hpp"

#include "gdml/objects.hpp"

#include "WireCellUtil/WireSchema.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>

using namespace WireCell;

namespace {
    double dist(const Point& a, const Point& b) { return (a - b).magnitude(); }
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::printf("usage: gdml_wires_check GDML [REFERENCE_WIRES|-] [readout=R] [sorter=S] [grouping=G]\n");
        return 2;
    }
    wcphlex::gdmlwires::Options opt;
    for (int i = 3; i < argc; ++i) {
        const std::string kv = argv[i];
        const auto eq = kv.find('=');
        const auto key = kv.substr(0, eq), val = eq == std::string::npos ? "" : kv.substr(eq + 1);
        if (key == "readout") opt.readout = val;
        else if (key == "sorter") opt.sorter = val;
        else if (key == "grouping") opt.grouping = val;
        else {
            std::printf("unknown option %s\n", argv[i]);
            return 2;
        }
    }
    auto t0 = std::chrono::steady_clock::now();
    auto doc = gdml::read(argv[1]);
    auto t1 = std::chrono::steady_clock::now();
    auto store = wcphlex::gdmlwires::wire_schema(doc, opt);
    auto t2 = std::chrono::steady_clock::now();
    auto drift = wcphlex::gdmlwires::drift_planes(doc, opt);
    auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
    std::set<int> chans;
    for (const auto& w : store.wires()) chans.insert(w.channel);
    std::printf("read %.0f ms, build %.0f ms: %zu anodes %zu faces %zu planes %zu wires %zu channels\n",
                ms(t0, t1), ms(t1, t2), store.anodes().size(), store.faces().size(), store.planes().size(),
                store.wires().size(), chans.size());
    std::map<std::tuple<double, double, int>, int> dvals;
    for (const auto& fd : drift) {
        ++dvals[{std::round(fd.collection * 100) / 100, std::round(fd.cathode * 100) / 100, fd.drift_sign}];
    }
    for (const auto& [k, n] : dvals) {
        std::printf("drift: collection %.2f mm cathode %.2f mm sign %+d : %d faces\n", std::get<0>(k),
                    std::get<1>(k), std::get<2>(k), n);
    }
    if (argc < 3 || std::string(argv[2]) == "-") return 0;

    auto ref = WireSchema::load(argv[2], WireSchema::Correction::pitch);
    int nbad = 0;
    double maxd = 0;
    size_t ncmp = 0;
    auto bad = [&](const std::string& m) {
        if (nbad++ < 20 || m.find("ref (") != std::string::npos) std::printf("MISMATCH %s\n", m.c_str());
    };
    if (ref.anodes().size() != store.anodes().size()) bad("anode count");
    for (const auto& ra : ref.anodes()) {
        const WireSchema::Anode* ga = nullptr;
        for (const auto& a : store.anodes())
            if (a.ident == ra.ident) ga = &a;
        if (!ga) { bad("no anode " + std::to_string(ra.ident)); continue; }
        auto rfaces = ref.faces(ra);
        auto gfaces = store.faces(*ga);
        if (rfaces.size() != gfaces.size()) { bad("face count anode " + std::to_string(ra.ident)); continue; }
        for (size_t f = 0; f < rfaces.size(); ++f) {
            // Match faces by ident: the order of an anode's faces is not
            // significant (files differ in it).
            const WireSchema::Face* gface = nullptr;
            for (const auto& gf : gfaces)
                if (gf.ident == rfaces[f].ident) gface = &gf;
            if (!gface) { bad("no face ident " + std::to_string(rfaces[f].ident) + " in anode " + std::to_string(ra.ident)); continue; }
            auto rplanes = ref.planes(rfaces[f]);
            auto gplanes = store.planes(*gface);
            if (rplanes.size() != gplanes.size()) { bad("plane count"); continue; }
            for (size_t p = 0; p < rplanes.size(); ++p) {
                const std::string where = "anode " + std::to_string(ra.ident) + " face " +
                                          std::to_string(rfaces[f].ident) + " plane " + std::to_string(rplanes[p].ident);
                if (rplanes[p].ident != gplanes[p].ident) bad("plane ident " + where);
                auto rw = ref.wires(rplanes[p]);
                auto gw = store.wires(gplanes[p]);
                if (rw.size() != gw.size()) { bad("wire count " + where); continue; }
                for (size_t i = 0; i < rw.size(); ++i) {
                    ++ncmp;
                    if (rw[i].ident != gw[i].ident || rw[i].channel != gw[i].channel ||
                        rw[i].segment != gw[i].segment) {
                        bad(where + " wire " + std::to_string(i) + ": ref (ident,chan,seg)=(" +
                            std::to_string(rw[i].ident) + "," + std::to_string(rw[i].channel) + "," +
                            std::to_string(rw[i].segment) + ") got (" + std::to_string(gw[i].ident) + "," +
                            std::to_string(gw[i].channel) + "," + std::to_string(gw[i].segment) + ")");
                    }
                    const double d = std::max(dist(rw[i].tail, gw[i].tail), dist(rw[i].head, gw[i].head));
                    maxd = std::max(maxd, d);
                    if (d > 0.05) bad(where + " wire " + std::to_string(i) + " endpoints differ by " + std::to_string(d) + " mm");
                }
            }
        }
    }
    std::printf("compared %zu wires: %d mismatches, max endpoint difference %.4f mm\n", ncmp, nbad, maxd);
    return nbad ? 1 : 0;
}
