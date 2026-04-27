// cfg/dune/phlex/job/sigproc.jsonnet
//
// PHLEX workflow builder: frame source → OmnibusSigProc → frame sink.
//
// Runs signal processing on digitized ADC frames (digits → signals).
// Wraps dune/wct/job/sigproc.jsonnet for use in the PHLEX framework.
//
// Usage (main workflow Jsonnet):
//   local build = import "dune/phlex/job/sigproc.jsonnet";
//   build({detname: "pdhd"}, input_file="digits.npz", output_file="signals.npz", anode_index=0)
//
// Arguments:
//   params        (object):  must contain params.detname
//   input_file    (string):  path to input NPZ file of digitized frames
//   output_file   (string):  path for output NPZ file of signal frames
//   anode_index   (integer): which anode from det.anodes[] to process (default 0)
//   nevents       (integer): number of events to process (default 1)
//   wct_log_sink  (string):  WCT log destination: "stdout", "stderr", or a file path (default: no logging)
//   wct_log_level (string):  WCT log level: "warn", "info", "debug", etc. (default: no level set)

function(
    params,
    input_file    = "digits.npz",
    output_file   = "signals.npz",
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
        frame_source: with_log({
            cpp:          "wcp_frame_source_file",
            wct_config:   "frame-file-source.jsonnet",
            wct_plugins:  ["WireCellPgraph", "WireCellSio"],
            output_layer: "event",
            wct_tla:      { inname: input_file },
        }),
    },

    modules: {
        frame_sigproc: with_log({
            cpp:         "wcp_frame_filter",
            wct_config:  "dune/wct/job/sigproc.jsonnet",
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
            input_from:  "frame_sigproc",
            wct_tla:     { outname: output_file },
        }),
    },
}
