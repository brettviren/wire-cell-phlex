// cfg/dune/phlex/job/sim-sigproc.jsonnet
//
// PHLEX workflow builder: DepoSet source → per-anode sim+sigproc → frame sink.
//
// Returns a PHLEX workflow object (suitable for `phlex -c`).  For multi-anode
// detectors the caller iterates over anodes and assembles per-anode modules;
// this function builds a single-anode workflow as the primary building block.
//
// Usage (main workflow Jsonnet, outside cfg/dune/):
//
//   // Option A: inline params
//   local build = import "dune/phlex/job/sim-sigproc.jsonnet";
//   build({detname: "pdhd"}, depo_file="my-depos.npz", output_file="out.npz", anode_index=0)
//
//   // Option B: params-file trick (no CLI TLA support in phlex)
//   local params = import "wire-cell-phlex-detector-params.jsonnet";
//   local build  = import "dune/phlex/job/sim-sigproc.jsonnet";
//   build(params, depo_file="muon-depos.npz", output_file="frames.npz", anode_index=0)
//
// Arguments:
//   params       (object):  must contain params.detname; may contain params.lar etc.
//   depo_file    (string):  path to input NPZ file of energy depositions
//   output_file  (string):  path for output NPZ file of signal-processed frames
//   anode_index  (integer): which anode from det.anodes[] to simulate (default 0)
//   nevents      (integer): number of events to process (default 1)

local dets = import "dune/wct/dets.jsonnet";

function(
    params,
    depo_file   = "muon-depos.npz",
    output_file = "sim-sigproc-frames.npz",
    anode_index = 0,
    nevents     = 1,
)

local det     = dets[params.detname](params);
local ai_str  = "%d" % anode_index;  // PHLEX wct_tla requires string values

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
        // Drift simulation + signal processing for the selected anode.
        // The wct_config is the detector-agnostic combined sim+sigproc job
        // function; the detector and anode are selected via wct_tla strings.
        frame_sim_sigproc: {
            cpp:         "wcp_deposet_to_frame",
            wct_config:  "dune/wct/job/sim-sigproc.jsonnet",
            wct_plugins: ["WireCellPgraph", "WireCellGen", "WireCellSigProc", "WireCellAux"],
            input_layer: "event",
            wct_tla: {
                detector:    params.detname,
                anode_index: ai_str,
            },
        },

        // Write the signal-processed frames to an NPZ file.
        frame_sink: {
            cpp:         "wcp_frame_sink_file",
            wct_config:  "frame-file-sink.jsonnet",
            wct_plugins: ["WireCellPgraph", "WireCellSio"],
            input_layer: "event",
            input_from:  "frame_sim_sigproc",
            wct_tla:     { outname: output_file },
        },
    },
}
