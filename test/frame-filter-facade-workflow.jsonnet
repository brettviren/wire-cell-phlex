{
  // PHLEX integration test workflow: geometry → frame source → WCT frame
  // passthrough with FacadeWireSchema → validation observer (Step 9).
  //
  // Exercises the deferred WCT initialization path:
  //   1. wcph_wire_schema_source loads a real wire geometry file once at the
  //      job layer and produces a wcphlex::WireSchema product.
  //   2. wcph_frame_source produces synthetic frames at the event layer.
  //   3. wcph_frame_filter (use_wire_schema: true) consumes both products.
  //      On the first event, it calls FacadeWireSchema::register_store() to
  //      deposit the geometry in the static map, then calls initialize().
  //      During WCT configure(), FacadeWireSchema reads from the map and
  //      exposes the store as an IWireSchema service.  WireSchemaValidator
  //      confirms the store is non-empty.  Subsequent events skip re-init.
  //   4. wcph_frame_observer asserts the output Frame is non-null.
  //
  // Requires WIRECELL_PATH to include wire-cell-phlex/cfg/ AND the WCT
  // shared data directory (for the wire geometry file).

  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 3, starting_number: 1 },
    },
  },
  sources: {
    geo: {
      cpp: 'wcph_wire_schema_source',
      output_layer: 'job',
      wire_schema_file: 'microboone-celltree-wires-v2.1.json.bz2',
    },
    frame_source: {
      cpp: 'wcph_frame_gen',
      output_layer: 'event',
    },
  },
  modules: {
    sigproc: {
      cpp: 'wcph_frame_filter',
      wct_config: 'frame-passthrough-with-facade.jsonnet',
      wct_plugins: ['WireCellPgraph'],
      input_layer: 'event',
      input_from: 'input',
      use_wire_schema: true,
    },
    verify: {
      cpp: 'wcph_frame_observer',
      input_layer: 'event',
      input_from: 'sigproc',
    },
  },
}
