{
    deposet(filename): {
        cpp:          "wcph_deposet_source_file",
        wct_config:   "deposet-file-source.jsonnet",
        wct_plugins:  ["WireCellPgraph", "WireCellSio"],
        output_layer: "event",
        wct_tla:      { inname: filename },
    },
    frame(filename): {
        cpp:          "wcph_frame_source_file",
        wct_config:   "frame-file-source.jsonnet",
        wct_plugins:  ["WireCellPgraph", "WireCellSio"],
        output_layer: "event",
        wct_tla:      { inname: filename },
    }
}


    
