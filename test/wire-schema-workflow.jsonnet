{
  // PHLEX job-layer geometry test (Step 8).
  //
  // wcph_wire_schema_source loads a WCT wire geometry file once at the job layer.
  // wcph_wire_schema_observer validates the result: non-null store, non-empty wires.
  //
  // Key design points exercised:
  //   - Job-layer source: runs once per job (not per event).
  //   - Direct WCT file loading via WireCell::WireSchema::load() without a
  //     WireCell::Main instance — only WIRECELL_PATH is required.
  //   - Job-layer observer consuming the same layer product.

  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 1, starting_number: 1 },
    },
  },
  sources: {
    geo: {
      cpp: 'wcph_wire_schema_source',
      output_layer: 'job',
      wire_schema_file: 'microboone-celltree-wires-v2.1.json.bz2',
    },
  },
  modules: {
    verify: {
      cpp: 'wcph_wire_schema_observer',
      input_layer: 'job',
    },
  },
}
