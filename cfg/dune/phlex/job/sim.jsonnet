// cfg/dune/phlex/job/sim.jsonnet
//
// PHLEX workflow builder: DepoSet source → drift+sim+digitize → frame sink.
//
// Produces digitized ADC frames (digits) from energy depositions.
// Wraps dune/wct/job/sim.jsonnet for use in the PHLEX framework.
//
// Usage (main workflow Jsonnet):
//   local build = import "dune/phlex/job/sim.jsonnet";
//   build({detname: "pdhd"}, depo_file="depos.npz", output_file="digits.npz", anode_index=0)
//
// Arguments:
//   params       (object):  must contain params.detname
//   depo_file    (string):  path to input NPZ file of energy depositions
//   output_file  (string):  path for output NPZ file of digitized frames
//   anode_index  (integer): which anode from det.anodes[] to simulate (default 0)
//   nevents      (integer): number of events to process (default 1)

function(
    params,
    depo_file   = "depos.npz",
    output_file = "digits.npz",
    anode_index = 0,
    nevents     = 1,
)

local ai_str = "%d" % anode_index;

{
    driver: {
        cpp: "generate_layers",
        layers: {
            event: { parent: "job", total: nevents, starting_number: 1 },
        },
    },

    sources: {
        deposet_source: {
            cpp:          "wcp_deposet_source_file",
            wct_config:   "deposet-file-source.jsonnet",
            wct_plugins:  ["WireCellPgraph", "WireCellSio"],
            output_layer: "event",
            wct_tla:      { inname: depo_file },
        },
    },

    modules: {
        frame_sim: {
            cpp:         "wcp_deposet_to_frame",
            wct_config:  "dune/wct/job/sim.jsonnet",
            wct_plugins: ["WireCellPgraph", "WireCellGen", "WireCellSigProc", "WireCellAux"],
            input_layer: "event",
            wct_tla: {
                detector:    params.detname,
                anode_index: ai_str,
            },
        },

        frame_sink: {
            cpp:         "wcp_frame_sink_file",
            wct_config:  "frame-file-sink.jsonnet",
            wct_plugins: ["WireCellPgraph", "WireCellSio"],
            input_layer: "event",
            input_from:  "frame_sim",
            wct_tla:     { outname: output_file },
        },
    },
}
