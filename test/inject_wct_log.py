#!/usr/bin/env python3
"""
inject_wct_log.py  WORKFLOW_JSON  LOG_FILE

Post-process a PHLEX workflow JSON file (as produced by wcsonnet) to inject
WCT logging parameters into every source and module entry that contains a
wct_config key.  The file is modified in-place.

Injected keys:
  wct_log_sink  — path to the log file (the LOG_FILE argument)
  wct_log_level — "debug"

This allows phlex to route all WCT diagnostic output (component configure()
calls, wire counts, channel ranges, etc.) to a per-job log file rather than
mixing it with Snakemake's stdout.
"""

import json
import sys


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} WORKFLOW_JSON LOG_FILE", file=sys.stderr)
        sys.exit(2)

    workflow_json = sys.argv[1]
    log_file = sys.argv[2]

    with open(workflow_json) as f:
        wf = json.load(f)

    for section in ("sources", "modules"):
        for entry in wf.get(section, {}).values():
            if "wct_config" in entry:
                entry["wct_log_sink"] = log_file
                entry["wct_log_level"] = "debug"

    with open(workflow_json, "w") as f:
        json.dump(wf, f, indent=2)


if __name__ == "__main__":
    main()
