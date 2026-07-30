// cfg/dune/phlex/job/segments-sim-sigproc.jsonnet
//
// Detector-independent FULL-CHAIN job builder (xerosere ddm-69y.7):
//
//   hmp_gen_event_gun -> esp_tracking (edep-sim, GDML) -> esp_observables
//   -> edep_segments_to_tracksegmentset
//   -> wcph_tracksegmentset_to_frame (dune/wct/job/sim-sigproc.jsonnet with
//      input=tracksegmentset: TrackSegmentSampler -> drift -> sim -> sigproc)
//   -> wc.frame TableGroup -> HDF5.
//
// Parameters:
//   params       (object):  detector object; only .detname is used here --
//                the WCT layer resolves the full detector via dune/wct/dets.
//   gdml         (string):  edep-sim GDML file path (must describe the SAME
//                detector as params.detname; see xerosere ddm-69y.6 notes).
//   gun          (object):  hmp_gen_event_gun parameter overrides.
//   output_file  (string):  HDF5 output path.
//   anode_index  (integer): anode to simulate.
//   nevents      (integer): number of events.

function(
    params,
    gdml,
    gun         = {},
    output_file = "segments-sim-sigproc.h5",
    anode_index = 0,
    nevents     = 1,
)

local ai_str = "%d" % anode_index;  // PHLEX wct_tla requires string values

{
    driver: {
        cpp: "generate_layers",
        layers: {
            event: { parent: "job", total: nevents, starting_number: 0 },
        },
    },

    sources: {
        gun: {
            cpp: "hmp_gen_event_gun",
            output_layer: "event",
            pdg: 13,
            mass: 105.658,
            energy: 1000.0,
            direction: [0, 0, 1],
            position: [0, 0, 0],
            number: 1,
            momentum_unit: "MeV",
            length_unit: "mm",
        } + gun,
    },

    modules: {
        edep_sim_tracking: {
            cpp: "esp_tracking",
            input_layer: "event",
            gdml: gdml,
        },
        edep_observables: {
            cpp: "esp_observables",
            input_layer: "event",
            input_from: "edep_sim_tracking",
        },
        segments_to_tracksegmentset: {
            cpp: "wire_cell_phlex_arrow_edep_segments",
            input_layer: "event",
        },
        frame_sim_sigproc: {
            cpp: "wcph_tracksegmentset_to_frame",
            wct_config: "dune/wct/job/sim-sigproc.jsonnet",
            wct_plugins: ["WireCellPgraph", "WireCellGen", "WireCellSigProc", "WireCellAux"],
            inputs: [{ creator: "segments_to_tracksegmentset", layer: "event",
                       suffix: "tracksegmentset" }],
            outputs: [{ suffix: "frame" }],
            wct_tla: {
                detector: params.detname,
                anode_index: ai_str,
                input: "tracksegmentset",
            },
        },
        to_arrow_frame: {
            cpp: "wire_cell_phlex_arrow_convert",
            input_layer: "event",
            input_creator: "frame_sim_sigproc",
            types: ["frame"],
        },
        hdf_out: {
            cpp: "phlex_arrow_hdf_output",
            output_file: output_file,
        },
    },
}
