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

// modules/frame_to_blobsets.cpp
//
// PHLEX algorithm module: IFrame -> DataVector<IBlobSet> "collect" node — the
// WCT 3D-imaging island.
//
// The WCT imaging sub-graph (MaskSlices -> SliceFanout -> GridTiling x2 ->
// BlobSetSync, see cfg/dune/wct/job/img.jsonnet) turns one input IFrame into a
// STREAM of IBlobSet, one per time slice.  CollectExecutor<IFrame,IBlobSet>
// drains the whole stream (queued by the BlobSetBoundarySink) into a single
// Phlex product wcphlex::BlobSets (= DataVector<IBlobSet>), which a downstream
// wire_cell_phlex_arrow_convert taps to the "wc.blobs" Arrow schema.
//
// Naming convention (register_shapes.hpp): the item type <blobset> pluralised,
// so the module library is libwcph_frame_to_blobsets.so and the node is
// registered as "wcph_frame_to_blobsets".  The output product suffix defaults to
// "blobsets"; the SPDIR config overrides it to "blobs" to match the convert
// module's wc.blobs type token.
//
// Config keys: wct_config (required), inputs/outputs arrays (1 each),
// wct_plugins / wct_app / wct_tla (optional).  Imaging needs the WireCellImg
// plugin (MaskSlices/SliceFanout/GridTiling/BlobSetSync) plus WireCellGen /
// WireCellSigProc / WireCellAux / WireCellPgraph.

#include "wire_cell_phlex/Data.hpp"

#include "modules/register_shapes.hpp"

#include "phlex/module.hpp"

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    wcphlex::register_collect<WireCell::IFrame, WireCell::IBlobSet>(m, config);
}
