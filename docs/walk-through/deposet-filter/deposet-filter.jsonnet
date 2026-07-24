// deposet-filter.jsonnet
//
// A self-contained Phlex workflow authored with the phlex.libsonnet DSL, with
// wcph_deposet_filter as the central ingredient:
//
//     deposet_source (provider)  --deposet-->  deposet_filter (transform)
//                                                     |
//                                              (--deposet-->  deposet_observer)
//
// Run it with `phlexed` (which resolves the "phlex/..." import via -J); see
// run.sh.  The document index.html walks through every line.

local phlex = import 'phlex/phlex.libsonnet';

// A synthetic source of DepoSets, one per "event" data cell (wcph_deposet_gen
// makes an empty SimpleDepoSet in-memory — the WCT-graph file reader is the
// separate wcph_deposet_source).  A Phlex provider stamps its products with
// creator "input" (a convention), which is what a consumer selects —
// src.output('deposet') encodes exactly that.
local src = phlex.source('deposet_source', 'wcph_deposet_gen',
                         layer='event', outputs=['deposet']);

// The DepoSetFilter node: a 1-in/1-out transform that runs the incoming DepoSet
// through a WCT sub-graph (deposet-passthrough.jsonnet) and emits the result.
//   - inputs : one product family, wired from the source's output.
//   - outputs: one product suffix ('deposet'); its creator becomes this node's
//              label, 'deposet_filter'.
//   - config : algorithm-specific keys merged at top level (the WCT settings).
local filt = phlex.node('deposet_filter', 'wcph_deposet_filter', layer='event',
                        inputs=[src.output('deposet')],
                        outputs=['deposet'],
                        config={
                          wct_config: 'deposet-passthrough.jsonnet',
                          wct_plugins: ['WireCellPgraph'],
                        });

// Assemble { driver, sources, modules }.  The result is a plain object, so we
// can extend it with `+` — here to add a not-yet-migrated observer node that
// still uses the flat legacy keys.  It coexists fine: Phlex forms edges by
// matching product descriptors, not by whether a node uses the new scheme.
phlex.workflow(
  phlex.generate_layers({ event: { parent: 'job', total: 3, starting_number: 1 } }),
  nodes=[filt],
  sources=[src],
) + {
  modules+: {
    deposet_observer: {
      cpp: 'wcph_deposet_observer',
      input_layer: 'event',
      input_from: 'deposet_filter',   // selects the filter's output (creator = its label)
    },
  },
}
