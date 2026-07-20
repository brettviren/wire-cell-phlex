#!/usr/bin/env bash
#
# run.sh — exercise the deposet-filter walk-through workflow.
#
# Authors the workflow with phlex.libsonnet and runs it via `phlexed`, which —
# unlike plain `phlex -c` — resolves the "phlex/phlex.libsonnet" import through
# a Jsonnet search path (-J).  `phlexed` is unofficial; it is used here (a doc /
# top-level demo) but never in a package-level test.
#
# Usage:
#   ./run.sh [env]
#     env   Spack environment under extern/envs/   (default: gcc15)
#
# Exit: 0 = ran to completion, 77 = skipped (env/build not present), else fail.

set -uo pipefail
SKIP=77

here="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
# walk-through dir -> repo root is five levels up:
#   devel/wire-cell-phlex/docs/walk-through/deposet-filter
root="$(cd "$here/../../../../.." && pwd)"

say()  { echo "[walk-through] $*" >&2; }
skip() { say "SKIP: $*"; exit "$SKIP"; }
die()  { say "ERROR: $*"; exit 1; }

env_name="${1:-gcc15}"
view="$root/extern/envs/$env_name/view"
install="$root/installs/envs/$env_name"
build="$root/builds/envs/$env_name/wire-cell-phlex"

phlexed="$install/bin/phlexed"
libsonnet="$install/share/jsonnet/phlex/phlex.libsonnet"

[ -x "$phlexed" ]                         || skip "phlexed not installed ($phlexed)"
[ -e "$libsonnet" ]                       || skip "phlex.libsonnet not installed ($libsonnet)"
[ -e "$build/libwcph_deposet_filter.so" ] || skip "wire-cell-phlex plugins not built ($build)"

# Plugins: wire-cell-phlex modules (build tree) + phlex framework plugins (the
# generate_layers driver) from the view.
export PHLEX_PLUGIN_PATH="$build:$view/lib"
# WCT resolves wct_config (deposet-passthrough.jsonnet) via WIRECELL_PATH; the
# copy in this directory makes the example self-contained.
export WIRECELL_PATH="$here"
export LD_LIBRARY_PATH="$build:$install/lib:$install/lib64:$view/lib:$view/lib64:${LD_LIBRARY_PATH:-}"

# -J <share/jsonnet> lets `import "phlex/phlex.libsonnet"` resolve.
say "running: phlexed -J $install/share/jsonnet -c deposet-filter.jsonnet"
"$phlexed" -J "$install/share/jsonnet" -c "$here/deposet-filter.jsonnet" || die "phlexed failed"
say "OK"
