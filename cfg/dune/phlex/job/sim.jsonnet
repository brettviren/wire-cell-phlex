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
//   params        (object):  must contain params.detname
//   depo_file     (string):  path to input NPZ file of energy depositions
//   output_file   (string):  path for output NPZ file of digitized frames
//   anode_index   (integer): which anode from det.anodes[] to simulate (default 0)
//   nevents       (integer): number of events to process (default 1)
//   wct_log_sink  (string):  WCT log destination: "stdout", "stderr", or a file path (default: no logging)
//   wct_log_level (string):  WCT log level: "warn", "info", "debug", etc. (default: no level set)

local sinks = import "sinks.jsonnet";
local sources = import "sources.jsonnet";

function(
    params,
    depo_file     = "depos.npz",
    output_file   = "digits.npz",
    anode_index   = 0,
    nevents       = 1,
    wct_log_sink  = "",
    wct_log_level = "",
)

local ai_str = "%d" % anode_index;

// Conditionally add wct_log_sink / wct_log_level to an entry.
local with_log = {
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
        deposet_source: sources.deposet(depo_file)
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
        } + with_log,

        frame_sink: sinks.frame(output_file),
    },
}
