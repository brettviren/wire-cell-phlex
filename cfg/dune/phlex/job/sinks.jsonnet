{
    frame(filename): {
        cpp:         "wcp_frame_sink_file",
        wct_config:  "frame-file-sink.jsonnet",
        wct_plugins: ["WireCellPgraph", "WireCellSio"],
        input_layer: "event",
        input_from:  "frame_sigproc",
        wct_tla:     { outname: filename },
    },
}

    
