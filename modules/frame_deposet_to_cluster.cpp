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

// modules/frame_deposet_to_cluster.cpp
//
// PHLEX algorithm module: (IFrame, IDepoSet) -> ICluster JOIN node — the "true"
// charge 3D-imaging island for a DepoFluxSplat frame.
//
// The WCT sub-graph (img.jsonnet, charge='depofill') tiles the frame into blobs
// and clusters them, then BlobDepoFill assigns TRUE charge from the drifted depos:
// MaskSlices/SumSlices -> SliceFanout -> GridTiling x2 -> BlobSetSync ->
// BlobClustering -> BlobDepoFill(cluster port 0, drifted IDepoSet port 1) ->
// ICluster.  The two inputs cross two Phlex boundary sources (IFrame + IDepoSet);
// the drifted depos are the same product the shared drift island produces.
//
// Node "wcph_frame_deposet_to_cluster"; two "inputs" selectors (frame, deposet)
// and one "outputs" element.  Needs WireCellImg (tiling/clustering/depofill) plus
// WireCellGen / WireCellAux / WireCellPgraph.

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_join<wcphlex::type_list<WireCell::IFrame, WireCell::IDepoSet>,
                           WireCell::ICluster>(m, config);
}
