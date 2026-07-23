{
    deposet(filename): {
        cpp:          "wcph_deposet_source",
        wct_config:   "deposet-file-source.jsonnet",
        wct_plugins:  ["WireCellPgraph", "WireCellSio"],
        inputs:       [],
        outputs:      [{ creator: "input", layer: "event", suffix: "deposet" }],
        wct_tla:      { inname: filename },
    },
    frame(filename): {
        cpp:          "wcph_frame_source",
        wct_config:   "frame-file-source.jsonnet",
        wct_plugins:  ["WireCellPgraph", "WireCellSio"],
        inputs:       [],
        outputs:      [{ creator: "input", layer: "event", suffix: "frame" }],
        wct_tla:      { inname: filename },
    }
}
