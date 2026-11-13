/*
 * This file is part of the Wire-Cell Toolkit.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wire_cell_phlex/GdmlWires.hpp"

#include "gdml/objects.hpp"
#include "gdml/placements.hpp"

#include "WireCellUtil/Exceptions.h"
#include "WireCellUtil/Logging.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <unordered_map>

using namespace WireCell;
using gdml::Vec3;

namespace wcphlex::gdmlwires {

    namespace {

        // LArSoft tolerances are in cm; ours are mm.
        constexpr double k_wire_tol = 1e-4 * 10;  // CRPWireReadoutGeom check_tolerance
        constexpr double k_tpc_tol = 1e-3 * 10;   // GeoObjectSorterCRU60D DistanceTol

        Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
        double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }
        Vec3 scale(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
        Point to_point(const Vec3& v) { return Point(v.x, v.y, v.z); }

        [[noreturn]] void fail(const std::string& msg)
        {
            THROW(ValueError() << errmsg{"gdmlwires: " + msg});
        }

        // A wire as LArSoft's WireGeo sees it before sorting: start/end at
        // the tube's local -/+ half length.
        struct WireG {
            Vec3 center, start, end;
            double thetaz{0};
        };

        double theta_z(const WireG& w)
        {
            const double half = norm(sub(w.end, w.center));
            double th = std::acos(std::clamp((w.end.z - w.center.z) / half, -1.0, 1.0));
            if (w.end.y < w.center.y) th *= -1.;
            if (th < 0) th += M_PI;
            return th;
        }

        // CRPWireReadoutSorter compareWireDX (drift along X).
        bool compare_wire_dx(const WireG& w1, const WireG& w2)
        {
            const auto& c1 = w1.center;
            const auto& c2 = w2.center;
            const Vec3 d = sub(w1.end, w1.start);
            double dy1 = d.y;
            const double dz1 = d.z;
            auto tol = [](double v) { return std::abs(v) < k_wire_tol; };
            if (tol(dz1)) return c1.z < c2.z;
            if (tol(dy1)) return c1.y < c2.y;
            if (w1.thetaz > 1.57) {  // u view
                if (tol(c2.z - c1.z)) {
                    if (dz1 < 0) dy1 = -dy1;
                    if (dy1 < 0) return c1.y < c2.y;
                    if (dy1 > 0) return c1.y > c2.y;
                }
                return c1.z < c2.z;
            }
            if (tol(c2.z - c1.z)) {  // v view
                if (dz1 < 0) dy1 = -dy1;
                if (dy1 < 0) return c1.y > c2.y;
                if (dy1 > 0) return c1.y < c2.y;
            }
            return c1.z > c2.z;
        }

        // dunecore WireReadoutSorterAPA::compareWires, on wire centers (as
        // written there, including exact comparisons).
        bool compare_wire_apa(const WireG& w1, const WireG& w2)
        {
            const auto& a = w1.center;
            const auto& b = w2.center;
            if (a.y > 0 && a.y > b.y) return true;
            if (a.y < 0 && a.y < b.y) return true;
            if (a.y == b.y && a.z < b.z) return true;
            return false;
        }

        struct PlaneG {
            Vec3 box_center;          // world
            std::vector<WireG> wires; // sorted
            // NearestWireID support
            Vec3 inc_dir;  // unit, from wire 0 toward wire 1, perpendicular to wires
            double pitch{0};

            int nearest_wire(const Vec3& pos) const
            {
                const double coord = dot(sub(pos, wires.front().center), inc_dir) / pitch;
                const int n = int(0.5 + coord);  // as PlaneGeo::NearestWireID
                if (n < 0 || n >= (int) wires.size()) {
                    fail("NearestWireID out of range: " + std::to_string(n));
                }
                return n;
            }
        };

        struct TpcG {
            Vec3 center;         // volTPC origin in the world
            Vec3 active_center;  // volTPCActive origin in the world
            double active_halfx{0};
            int drift_sign{0};   // along X
            std::vector<PlaneG> planes;  // sorted by drift distance
        };

        bool compare_tpc(const TpcG& t1, const TpcG& t2)  // GeoObjectSorterCRU60D, X drift
        {
            const auto& a = t1.center;
            const auto& b = t2.center;
            if (std::abs(a.x - b.x) > k_tpc_tol) return a.x < b.x;
            if (std::abs(a.z - b.z) > k_tpc_tol) return a.z < b.z;
            return a.y < b.y;
        }

        // GeoObjectSorterCRU60D with SortTPCPDVD (ProtoDUNE-VD), X drift:
        // drift volume, then the sign of Y, then Z, then Y.
        bool compare_tpc_pdvd(const TpcG& t1, const TpcG& t2)
        {
            const auto& a = t1.center;
            const auto& b = t2.center;
            if (std::abs(a.x - b.x) > k_tpc_tol) return a.x < b.x;
            if (a.y * b.y < 0) return a.y < b.y;
            if (std::abs(a.z - b.z) > k_tpc_tol) return a.z < b.z;
            return a.y < b.y;
        }

        // dunecore GeoObjectSorterAPA: compareIncreasingZYX of TPC centers
        // (so TPCs 2a and 2a+1 make APA a).
        bool compare_tpc_apa(const TpcG& t1, const TpcG& t2)
        {
            const auto& a = t1.center;
            const auto& b = t2.center;
            if (a.z != b.z) return a.z < b.z;
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        }

        bool is_tpc_name(const std::string& n)
        {
            return n.starts_with("volTPC") && !n.starts_with("volTPCActive") && !n.starts_with("volTPCPlane") &&
                   !n.starts_with("volTPCWire");
        }

        // A plane LV's wires in the plane frame (unsorted).
        std::vector<WireG> plane_wires_local(const gdml::Document& doc, gdml::Index plane_lv)
        {
            std::vector<WireG> ret;
            for (auto pvi : doc.volumes[plane_lv].physvols) {
                const auto& pv = doc.physvols[pvi];
                const auto& lv = doc.volumes[pv.volume];
                if (!lv.name.starts_with("volTPCWire")) continue;
                const auto& solid = doc.solids[lv.solid];
                if (solid.kind != gdml::SolidKind::tube) {
                    fail("wire volume " + lv.name + " is not a tube");
                }
                const double half = solid.at("z") / 2;  // GDML tube z is the full length
                const auto tr = gdml::local_transform(pv);
                WireG w;
                w.center = tr.apply({0, 0, 0});
                w.start = tr.apply({0, 0, -half});
                w.end = tr.apply({0, 0, half});
                ret.push_back(w);
            }
            return ret;
        }

        std::vector<TpcG> extract_tpcs(const gdml::Document& doc, const Options& opt)
        {
            std::vector<TpcG> tpcs;
            for (auto tpc_lv : gdml::find_volumes(doc, "volTPC")) {
                const auto& tlv = doc.volumes[tpc_lv];
                if (!is_tpc_name(tlv.name)) continue;

                // Active volume: a direct daughter volTPCActive*.
                gdml::Transform active_local;
                double active_half[3] = {0, 0, 0};
                bool have_active = false;
                for (auto pvi : tlv.physvols) {
                    const auto& pv = doc.physvols[pvi];
                    const auto& lv = doc.volumes[pv.volume];
                    if (lv.name.starts_with("volTPCActive")) {
                        active_local = gdml::local_transform(pv);
                        const auto& s = doc.solids[lv.solid];
                        if (s.kind != gdml::SolidKind::box) fail("active volume " + lv.name + " is not a box");
                        active_half[0] = s.at("x") / 2;
                        active_half[1] = s.at("y") / 2;
                        active_half[2] = s.at("z") / 2;
                        have_active = true;
                        break;
                    }
                }
                if (!have_active) fail("TPC volume " + tlv.name + " has no volTPCActive daughter");

                // Planes (any depth below the TPC) and their wires.
                struct PlaneL {
                    gdml::Transform tr;  // plane frame -> TPC frame
                    std::vector<WireG> wires;
                };
                std::vector<PlaneL> planes_local;
                for (auto plane_lv : gdml::find_volumes(doc, "volTPCPlane", tpc_lv)) {
                    auto wires = plane_wires_local(doc, plane_lv);
                    for (const auto& pl : gdml::placements(doc, plane_lv, tpc_lv)) {
                        planes_local.push_back({pl.transform, wires});
                    }
                }
                if (planes_local.empty()) fail("TPC volume " + tlv.name + " has no planes");

                for (const auto& tplace : gdml::placements(doc, tpc_lv)) {
                    const auto& T = tplace.transform;
                    TpcG t;
                    t.center = T.apply({0, 0, 0});
                    t.active_center = (T * active_local).apply({0, 0, 0});
                    // Active box half width along world X (the box may be rotated).
                    {
                        const auto R = (T * active_local).R;
                        t.active_halfx = std::abs(R[0]) * active_half[0] + std::abs(R[1]) * active_half[1] +
                                         std::abs(R[2]) * active_half[2];
                    }
                    for (const auto& pl : planes_local) {
                        const auto PT = T * pl.tr;
                        PlaneG p;
                        p.box_center = PT.apply({0, 0, 0});
                        for (const auto& wl : pl.wires) {
                            WireG w;
                            w.center = PT.apply(wl.center);
                            w.start = PT.apply(wl.start);
                            w.end = PT.apply(wl.end);
                            w.thetaz = theta_z(w);
                            p.wires.push_back(w);
                        }
                        t.planes.push_back(std::move(p));
                    }
                    // Drift axis: from the active center to a plane (LArSoft
                    // DriftAxisWithSign); only X is supported.
                    const Vec3 dv = sub(t.planes.front().box_center, t.active_center);
                    if (!(std::abs(dv.x) > std::abs(dv.y) && std::abs(dv.x) > std::abs(dv.z))) {
                        fail("TPC " + tlv.name + " does not drift along X");
                    }
                    t.drift_sign = dv.x > 0 ? +1 : -1;
                    // Planes by increasing drift distance from the TPC center.
                    const Vec3 c = t.center;
                    Vec3 axis = sub(t.planes.front().box_center, c);
                    axis = scale(axis, 1.0 / norm(axis));
                    std::sort(t.planes.begin(), t.planes.end(), [&](const PlaneG& a, const PlaneG& b) {
                        return dot(sub(a.box_center, c), axis) < dot(sub(b.box_center, c), axis);
                    });
                    for (auto& p : t.planes) {
                        if (p.wires.size() < 2) fail("plane with fewer than 2 wires");
                        if (opt.readout == "apa") {
                            std::sort(p.wires.begin(), p.wires.end(), compare_wire_apa);
                        }
                        else {
                            std::sort(p.wires.begin(), p.wires.end(), compare_wire_dx);
                        }
                        // NearestWireID support: pitch and increasing direction.
                        const auto& w0 = p.wires[0];
                        const auto& w1 = p.wires[1];
                        Vec3 wd = sub(w0.end, w0.start);
                        wd = scale(wd, 1.0 / norm(wd));
                        Vec3 d01 = sub(w1.center, w0.center);
                        Vec3 perp = sub(d01, scale(wd, dot(d01, wd)));
                        p.pitch = norm(perp);
                        p.inc_dir = scale(perp, 1.0 / p.pitch);
                    }
                    tpcs.push_back(std::move(t));
                }
            }
            if (tpcs.empty()) fail("no volTPC volumes placed in the world");
            if (opt.sorter == "cru60d-pdvd") {
                std::stable_sort(tpcs.begin(), tpcs.end(), compare_tpc_pdvd);
            }
            else if (opt.sorter == "apa") {
                std::stable_sort(tpcs.begin(), tpcs.end(), compare_tpc_apa);
            }
            else {
                std::stable_sort(tpcs.begin(), tpcs.end(), compare_tpc);
            }
            return tpcs;
        }

        // CRPWireReadoutGeom groupTPCsIntoSets: 2x2 blocks in (Y, Z).
        std::vector<std::array<int, 4>> group_sets(const std::vector<TpcG>& tpcs)
        {
            const int ntpc = (int) tpcs.size();
            if (ntpc % 4) fail("number of TPCs is not a multiple of 4");
            int ncells = 1;
            for (int i = 1; i < ntpc; ++i) {
                if (tpcs[i].drift_sign != tpcs[i - 1].drift_sign && ncells < 2) ++ncells;
            }
            const int ntpc_cell = ntpc / ncells;
            int nz = 0;
            const double z0 = tpcs[0].center.z;
            for (int i = 0; i < ntpc_cell; ++i) {
                if (std::abs(z0 - tpcs[i].center.z) < k_wire_tol) ++nz;
                else break;
            }
            const int no = ntpc / nz;
            if (nz % 2 || no % 2) fail("could not interpret the CRP geometry");
            std::vector<std::array<int, 4>> sets;
            for (int irow = 0; irow < no; irow += 2) {
                for (int icol = 0; icol < nz; icol += 2) {
                    const int i0 = icol + irow * nz, i2 = icol + (irow + 1) * nz;
                    sets.push_back({i0, i0 + 1, i2, i2 + 1});
                }
            }
            if ((int) sets.size() != ntpc / 4) fail("bad number of TPC sets");
            return sets;
        }

        void check_options(const Options& opt)
        {
            if (opt.readout != "crp" && opt.readout != "apa") {
                fail("unsupported readout '" + opt.readout + "' (want crp or apa)");
            }
            if (opt.sorter != "cru60d" && opt.sorter != "cru60d-pdvd" && opt.sorter != "apa") {
                fail("unsupported sorter '" + opt.sorter + "' (want cru60d, cru60d-pdvd or apa)");
            }
            if (opt.grouping != "pair-tpc") fail("unsupported grouping '" + opt.grouping + "' (want pair-tpc)");
        }

        std::pair<int, int> pair_tpc(int gtpc) { return {gtpc / 2, (1 + gtpc) % 2}; }

    }  // namespace

    namespace {

        // One (TPC, plane) contributing to an APA readout plane.
        struct TpcPlane {
            int tpc{0}, plane{0};
        };

        // dunecore DuneApaWireReadoutGeom: APA a = TPCs 2a and 2a+1; readout
        // planes per APA: U of both TPCs (wrapped), V of both (wrapped), Z of
        // TPC 2a, Z of TPC 2a+1.  A plane contributes its "anchored" wires to
        // the channel count: all for Z, and for U/V the wires before the first
        // pair of consecutive wires with equal center Z.  A channel's wires
        // alternate between the two TPCs, the wire index advancing by the
        // anchored count (ChannelToWire).
        void apa_channels(const std::vector<TpcG>& tpcs, WireSchema::StoreDB& db, int& nchannels, size_t& nwires)
        {
            const int ntpc = (int) tpcs.size();
            if (ntpc % 2) fail("APA readout needs an even number of TPCs");
            for (const auto& t : tpcs) {
                if (t.planes.size() != 3) fail("APA readout needs 3 planes per TPC");
            }
            std::vector<std::array<int, 3>> anchored(ntpc);
            for (int t = 0; t < ntpc; ++t) {
                for (int p = 0; p < 3; ++p) {
                    const auto& wires = tpcs[t].planes[p].wires;
                    const int nw = (int) wires.size();
                    int na = 0;
                    if (p == 2) {
                        na = nw;
                    }
                    else {
                        for (int i = 0; i + 1 < nw; ++i) {
                            if (std::abs(wires[i].center.z - wires[i + 1].center.z) < 1e-6) {
                                na = i;
                                break;
                            }
                        }
                        if (na == 0 && nw > 310 && nw < 320) na = 200;  // as dunecore
                    }
                    if (na == 0) fail("plane with no anchored wires");
                    anchored[t][p] = na;
                }
            }
            int icha = 0;
            nwires = 0;
            for (int a = 0; a < ntpc / 2; ++a) {
                const int t1 = 2 * a, t2 = 2 * a + 1;
                const std::vector<std::vector<TpcPlane>> rops{
                    {{t1, 0}, {t2, 0}}, {{t1, 1}, {t2, 1}}, {{t1, 2}}, {{t2, 2}}};
                for (const auto& rop : rops) {
                    const int first = icha;
                    for (const auto& tp : rop) icha += anchored[tp.tpc][tp.plane];
                    const int pl = rop[0].plane;
                    const int ta = rop[0].tpc;
                    const int tb = rop.back().tpc;
                    const bool wrapped = rop.size() == 2;
                    if (wrapped && anchored[ta][pl] != anchored[tb][pl]) {
                        fail("wrapped planes have different anchored wire counts");
                    }
                    for (int ch = first; ch < icha; ++ch) {
                        int t = ta;
                        int iw = ch - first;
                        if (wrapped && iw >= anchored[ta][pl]) {
                            t = tb;
                            iw -= anchored[ta][pl];
                        }
                        int segment = 0;
                        while (iw < (int) tpcs[t].planes[pl].wires.size()) {
                            const auto& wg = tpcs[t].planes[pl].wires[iw];
                            const auto [aid, fid] = pair_tpc(t);
                            WireSchema::Wire w;
                            w.ident = iw;
                            w.channel = ch;
                            w.segment = segment++;
                            w.tail = to_point(wg.start);
                            w.head = to_point(wg.end);
                            auto& plane = WireSchema::get_append(db, pl, fid, aid, 0);
                            db.wires.push_back(w);
                            plane.wires.push_back(db.wires.size() - 1);
                            ++nwires;
                            iw += anchored[t][pl];
                            t = (t == ta) ? tb : ta;
                        }
                    }
                }
            }
            nchannels = icha;
        }

    }  // namespace

    WireSchema::Store wire_schema(const gdml::Document& doc, const Options& opt)
    {
        check_options(opt);
        auto log = Log::logger("gdmlwires");
        const auto tpcs = extract_tpcs(doc, opt);
        if (opt.readout == "apa") {
            auto db = std::make_shared<WireSchema::StoreDB>();
            int nchannels = 0;
            size_t nwires = 0;
            apa_channels(tpcs, *db, nchannels, nwires);
            log->info("gdmlwires: {} TPCs, {} APAs, {} channels, {} wires", tpcs.size(), tpcs.size() / 2,
                      nchannels, nwires);
            WireSchema::correct(*db, static_cast<WireSchema::Correction>(opt.correction));
            return WireSchema::Store(db);
        }
        const auto sets = group_sets(tpcs);
        const int nplanes = (int) tpcs.front().planes.size();
        for (const auto& t : tpcs) {
            if ((int) t.planes.size() != nplanes) fail("TPCs differ in their number of planes");
        }

        // fillChannelToWireMap: channel ranges per (tpc, plane), ROPs per set.
        struct Range { int first{0}, end{0}; };
        std::vector<std::vector<Range>> range(tpcs.size(), std::vector<Range>(nplanes));
        struct Rop { int first{0}, end{0}; std::array<int, 4> tpcs; int plane{0}; };
        std::vector<Rop> rops;
        int next = 0;
        for (const auto& set : sets) {
            for (int r = 0; r < nplanes; ++r) {
                Rop rop{next, 0, set, r};
                Vec3 last = tpcs[set[0]].planes[r].wires.back().center;
                for (int k = 0; k < 4; ++k) {
                    const auto& plane = tpcs[set[k]].planes[r];
                    const int nw = (int) plane.wires.size();
                    int first = next;
                    if (r > 1) {
                        next += nw;
                    }
                    else if (k % 2 == 0) {
                        next += nw;
                    }
                    else {
                        const int m = plane.nearest_wire(last);
                        first = (next - 1) - m;
                        next = first + nw;
                    }
                    range[set[k]][r] = {first, next};
                    last = plane.wires.back().center;
                }
                rop.end = next;
                rops.push_back(rop);
            }
        }
        const int nchannels = next;

        // ChannelToWire for every channel, in channel order.
        auto db = std::make_shared<WireSchema::StoreDB>();
        size_t nwires = 0;
        for (const auto& rop : rops) {
            for (int ch = rop.first; ch < rop.end; ++ch) {
                int segment = 0;
                for (int k = 0; k < 4; ++k) {
                    const int t = rop.tpcs[k];
                    const auto& rg = range[t][rop.plane];
                    if (ch < rg.first || ch >= rg.end) continue;
                    const int wip = ch - rg.first;
                    const auto& wg = tpcs[t].planes[rop.plane].wires[wip];
                    const auto [aid, fid] = pair_tpc(t);
                    WireSchema::Wire w;
                    w.ident = wip;
                    w.channel = ch;
                    w.segment = segment++;
                    w.tail = to_point(wg.start);
                    w.head = to_point(wg.end);
                    auto& plane = WireSchema::get_append(*db, rop.plane, fid, aid, 0);
                    db->wires.push_back(w);
                    plane.wires.push_back(db->wires.size() - 1);
                    ++nwires;
                }
            }
        }
        log->info("gdmlwires: {} TPCs, {} TPC sets, {} channels, {} wires", tpcs.size(), sets.size(), nchannels,
                  nwires);

        WireSchema::correct(*db, static_cast<WireSchema::Correction>(opt.correction));
        return WireSchema::Store(db);
    }

    DriftPlanes drift_planes(const gdml::Document& doc, const Options& opt)
    {
        check_options(opt);
        const auto tpcs = extract_tpcs(doc, opt);
        DriftPlanes ret;
        for (size_t t = 0; t < tpcs.size(); ++t) {
            const auto& tpc = tpcs[t];
            DriftPlane fd;
            std::tie(fd.anode, fd.face) = pair_tpc((int) t);
            fd.collection = tpc.planes.back().box_center.x;  // the collection (last) plane
            fd.cathode = tpc.active_center.x - tpc.drift_sign * tpc.active_halfx;
            fd.drift_sign = tpc.drift_sign;
            ret.push_back(fd);
        }
        std::sort(ret.begin(), ret.end(),
                  [](const DriftPlane& a, const DriftPlane& b) { return std::tie(a.anode, a.face) < std::tie(b.anode, b.face); });
        return ret;
    }

}  // namespace wcphlex::gdmlwires
