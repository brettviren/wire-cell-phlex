// cfg/dune/phlex/job/splat.jsonnet
//
// PHLEX workflow builder: DepoSet source → drift+splat → frame sink.
//
// Produces "true signal" frames (splats) via DepoFluxSplat.  These serve as
// the truth reference for SPDIR metric comparisons against sim+sigproc output.
// Wraps dune/wct/job/splat.jsonnet for use in the PHLEX framework.
//
// Usage (main workflow Jsonnet):
//   local build = import "dune/phlex/job/splat.jsonnet";
//   build({detname: "pdhd"}, depo_file="depos.npz", output_file="splats.npz", anode_index=0)
//
// Arguments:
//   params        (object):  must contain params.detname
//   depo_file     (string):  path to input NPZ file of energy depositions
//   output_file   (string):  path for output NPZ file of splat frames
//   anode_index   (integer): which anode from det.anodes[] to process (default 0)
//   nevents       (integer): number of events to process (default 1)
//   wct_log_sink  (string):  WCT log destination: "stdout", "stderr", or a file path (default: no logging)
//   wct_log_level (string):  WCT log level: "warn", "info", "debug", etc. (default: no level set)

function(
    params,
    depo_file     = "depos.npz",
    output_file   = "splats.npz",
    anode_index   = 0,
    nevents       = 1,
    wct_log_sink  = "",
    wct_log_level = "",
)

local ai_str = "%d" % anode_index;

// Conditionally add wct_log_sink / wct_log_level to an entry.
local with_log(entry) = entry + {
    [if wct_log_sink  != "" then "wct_log_sink"]:  wct_log_sink,
    [if wct_log_level != "" then "wct_log_level"]: wct_log_level,
};

{
    driver: {
        cpp: "generate_layers",
        layers: {
            event: { parent: "job", total: nevents, starting_number: 1 },
        },
    },

    sources: {
        deposet_source: with_log({
            cpp:          "wcp_deposet_source_file",
            wct_config:   "deposet-file-source.jsonnet",
            wct_plugins:  ["WireCellPgraph", "WireCellSio"],
            output_layer: "event",
            wct_tla:      { inname: depo_file },
        }),
    },

    modules: {
        frame_splat: with_log({
            cpp:         "wcp_deposet_to_frame",
            wct_config:  "dune/wct/job/splat.jsonnet",
            wct_plugins: ["WireCellPgraph", "WireCellGen", "WireCellSigProc", "WireCellAux"],
            input_layer: "event",
            wct_tla: {
                detector:    params.detname,
                anode_index: ai_str,
            },
        }),

        frame_sink: with_log({
            cpp:         "wcp_frame_sink_file",
            wct_config:  "frame-file-sink.jsonnet",
            wct_plugins: ["WireCellPgraph", "WireCellSio"],
            input_layer: "event",
            input_from:  "frame_splat",
            wct_tla:     { outname: output_file },
        }),
    },
}
