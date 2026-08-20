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

// modules/frame_to_cluster.cpp
//
// PHLEX algorithm module: IFrame -> ICluster function node — the charged
// 3D-imaging island for a signal-processing frame.
//
// The WCT sub-graph (img.jsonnet, charge='solve') tiles the frame into blobs and
// then clusters + solves their charge: MaskSlices/SumSlices -> SliceFanout ->
// GridTiling x2 -> BlobSetSync -> BlobClustering -> BlobGrouping -> BlobSolving,
// crossing the Phlex boundary as one ICluster per frame (charge on the b-nodes).
// A downstream wire_cell_phlex_arrow_convert (types=['cluster']) extracts the
// charged blobs into the wc.blobs Arrow schema.
//
// Node "wcph_frame_to_cluster"; config keys as for the other shapes (wct_config,
// inputs/outputs, wct_plugins/wct_app/wct_tla).  Imaging needs WireCellImg
// (tiling + clustering + solving) plus WireCellGen / WireCellAux / WireCellPgraph.

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_function<WireCell::IFrame, WireCell::ICluster>(m, config);
}
