#!/usr/bin/env bash
#
# gdml-wires-ref.sh CHECK GDML REFERENCE [OPTIONS...]
#
# Run gdml_wires_check (CHECK) on GDML against the WCT wires file REFERENCE
# with the GdmlWires OPTIONS (readout=, sorter=, grouping=).  GDML is looked
# up on $GDML_PATH and REFERENCE on $WIRECELL_PATH unless given as paths;
# REFERENCE "-" runs CHECK on GDML alone.  Exits 77 (skip) if a file is not
# found.
set -u
check=$1 gdml=$2 ref=$3; shift 3
find_on() {  # NAME PATHVAR
    local name=$1 dirs=${!2:-}
    [ -e "$name" ] && { echo "$name"; return; }
    local IFS=:
    for d in $dirs; do [ -e "$d/$name" ] && { echo "$d/$name"; return; }; done
}
g=$(find_on "$gdml" GDML_PATH)
[ -n "$g" ] || { echo "SKIP: $gdml not on GDML_PATH"; exit 77; }
[ "$ref" = "-" ] && exec "$check" "$g" "$@"
r=$(find_on "$ref" WIRECELL_PATH)
[ -n "$r" ] || { echo "SKIP: $ref not on WIRECELL_PATH"; exit 77; }
exec "$check" "$g" "$r" "$@"
