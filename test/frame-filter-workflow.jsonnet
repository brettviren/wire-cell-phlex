{
  // PHLEX integration test workflow: frame source → WCT frame passthrough → done.
  //
  // wcph_frame_source produces 3 synthetic frames (ident = cell number 1..3).
  // wcph_frame_filter wraps FrameFilter backed by frame-passthrough.jsonnet,
  // which routes each frame through a trivial WCT Pgraph (src → sink).
  //
  // Requires WIRECELL_PATH to include wire-cell-phlex/cfg/ so that WCT can
  // locate frame-passthrough.jsonnet.
  //
  // Run with PHLEX_PLUGIN_PATH pointing to the build directory:
  //   WIRECELL_PATH=<src>/cfg PHLEX_PLUGIN_PATH=<build> phlex -c frame-filter-workflow.jsonnet

  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 3, starting_number: 1 },
    },
  },
  sources: {
    frame_source: {
      cpp: 'wcph_frame_gen',
      output_layer: 'event',
    },
  },
  modules: {
    frame_filter: {
      cpp: 'wcph_frame_filter',
      wct_config: 'frame-passthrough-generic.jsonnet',
      wct_plugins: ['WireCellPgraph'],
      inputs: [{ creator: 'input', layer: 'event', suffix: 'frame' }],
      outputs: [{ suffix: 'frame' }],
    },
  },
}
