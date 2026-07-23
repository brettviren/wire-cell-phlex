{
    frame(filename, input_from): {
        cpp:         "wcph_frame_sink",
        wct_config:  "frame-file-sink.jsonnet",
        wct_plugins: ["WireCellPgraph", "WireCellSio"],
        inputs:      [{ creator: input_from, layer: "event", suffix: "frame" }],
        outputs:     [],
        wct_tla:     { outname: filename },
    },
}
