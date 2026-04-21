{
  // PHLEX integration test workflow: deposet source → WCT deposet-to-frame → done.
  //
  // wcp_deposet_source produces 3 synthetic DepoSets (ident = cell number 1..3).
  // wcp_deposet_to_frame wraps DepoSetToFrame backed by deposet-to-frame.jsonnet,
  // which routes each DepoSet through TrivialDepoFramer and produces a Frame
  // with the same ident.
  //
  // Requires WIRECELL_PATH to include wire-cell-phlex/cfg/.

  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 3, starting_number: 1 },
    },
  },
  sources: {
    deposet_source: {
      cpp: 'wcp_deposet_source',
      output_layer: 'event',
    },
  },
  modules: {
    deposet_to_frame: {
      cpp: 'wcp_deposet_to_frame',
      wct_config: 'deposet-to-frame.jsonnet',
      wct_plugins: ['WireCellPgraph'],
      input_layer: 'event',
    },
  },
}
