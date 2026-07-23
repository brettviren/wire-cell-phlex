{
  // PHLEX multi-instance test (Step 7).
  //
  // Two independent wcph_frame_filter instances ("sigproc_a", "sigproc_b") each
  // process a separate input stream.  Their outputs are consumed by a two-frame
  // observer that asserts both frames are non-null and distinct.
  //
  // Key design points exercised:
  //   - Same 'cpp' library loaded under two module keys → independent PHLEX
  //     product namespaces (creator = "sigproc_a" vs "sigproc_b").
  //   - Each FrameFilter gets unique WCT component instance names derived from
  //     its module_label, preventing factory aliasing.
  //   - output_suffix differentiates the two source streams within the same layer.

  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 3, starting_number: 1 },
    },
  },
  sources: {
    source_a: {
      cpp: 'wcph_frame_gen',
      output_layer: 'event',
      output_suffix: 'frame_a',
    },
    source_b: {
      cpp: 'wcph_frame_gen',
      output_layer: 'event',
      output_suffix: 'frame_b',
    },
  },
  modules: {
    sigproc_a: {
      cpp: 'wcph_frame_filter',
      wct_config: 'frame-passthrough-generic.jsonnet',
      wct_plugins: ['WireCellPgraph'],
      inputs: [{ creator: 'input', layer: 'event', suffix: 'frame_a' }],
      outputs: [{ suffix: 'frame' }],
    },
    sigproc_b: {
      cpp: 'wcph_frame_filter',
      wct_config: 'frame-passthrough-generic.jsonnet',
      wct_plugins: ['WireCellPgraph'],
      inputs: [{ creator: 'input', layer: 'event', suffix: 'frame_b' }],
      outputs: [{ suffix: 'frame' }],
    },
    verify: {
      cpp: 'wcph_two_frame_observer',
      input_layer: 'event',
      input_from_a: 'sigproc_a',
      input_from_b: 'sigproc_b',
    },
  },
}
