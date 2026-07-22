{
  // PHLEX integration test workflow: deposet source → WCT deposet filter → observer.
  //
  // wcph_deposet_source produces 3 synthetic DepoSets (ident = cell number 1..3).
  // wcph_deposet_filter wraps DepoSetFilter backed by deposet-passthrough.jsonnet,
  // which routes each DepoSet straight through (identity transform).
  // wcph_deposet_observer asserts the output DepoSet is non-null for every event.
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
      cpp: 'wcph_deposet_source',
      output_layer: 'event',
    },
  },
  modules: {
    deposet_filter: {
      cpp: 'wcph_deposet_filter',
      wct_config: 'deposet-passthrough-generic.jsonnet',
      wct_plugins: ['WireCellPgraph'],
      input_layer: 'event',
      input_from: 'input',
    },
    deposet_observer: {
      cpp: 'wcph_deposet_observer',
      input_layer: 'event',
      input_from: 'deposet_filter',
    },
  },
}
