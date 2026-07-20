{
    frame(filename, input_from): {
        cpp:         "wcph_frame_sink_file",
        wct_config:  "frame-file-sink.jsonnet",
        wct_plugins: ["WireCellPgraph", "WireCellSio"],
        input_layer: "event",
        input_from:  input_from,
        wct_tla:     { outname: filename },
    },
}
