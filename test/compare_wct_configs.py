#!/usr/bin/env python3
"""
compare_wct_configs.py  [options]  FILE_A  FILE_B

Compare two WCT configuration JSON arrays (as produced by evaluating a
Jsonnet config with `jsonnet`).  Used to validate that the new factored
wire-cell-phlex configs produce equivalent WCT component configurations
compared to the dunereco reference configs.

Exit codes:
  0  all checks passed (within tolerances)
  1  one or more checks failed
  2  usage error or file not found

Comparison strategy:
  - Parse both files as JSON arrays of WCT component objects: {type, name, data}
  - Match objects by (type, name) key
  - Report objects present in A only or B only (with an optional allow-list for
    expected differences: e.g. art wcls input/output nodes in dunereco, boundary
    nodes in phlex)
  - For matched objects, compare data fields recursively

Options:
  --skip-type TYPE       Do not report mismatches for objects with this WCT type.
                         Can be repeated.  Use for art-specific types that have no
                         phlex equivalent (e.g. "wclsFrameSaver", "SimEnergyDeposit").
  --skip-name NAME       Do not report mismatches for objects with this instance name.
  --float-tol TOL        Relative tolerance for floating-point comparisons (default 1e-6).
  --show-match           Also print PASS lines for matched objects.
"""

import sys
import json
import argparse
import math


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("file_a")
    p.add_argument("file_b")
    p.add_argument("--skip-type", action="append", default=[], metavar="TYPE",
                   help="Skip objects with this WCT type (can repeat)")
    p.add_argument("--skip-name", action="append", default=[], metavar="NAME",
                   help="Skip objects with this instance name (can repeat)")
    p.add_argument("--float-tol", type=float, default=1e-6,
                   help="Relative tolerance for float comparisons (default 1e-6)")
    p.add_argument("--show-match", action="store_true",
                   help="Print PASS for matched objects")
    return p.parse_args()


def load_json(path):
    try:
        with open(path) as f:
            return json.load(f)
    except Exception as e:
        print(f"ERROR loading {path}: {e}", file=sys.stderr)
        sys.exit(2)


def index_by_key(components):
    """Return dict keyed by (type, name) → component."""
    idx = {}
    for c in components:
        key = (c.get("type", ""), c.get("name", ""))
        if key in idx:
            print(f"WARNING: duplicate (type,name) key {key}", file=sys.stderr)
        idx[key] = c
    return idx


def floats_close(a, b, tol):
    if a == b:
        return True
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        if a == 0 and b == 0:
            return True
        denom = max(abs(a), abs(b))
        if denom == 0:
            return a == b
        return abs(a - b) / denom <= tol
    return False


def diff_values(path, va, vb, tol, diffs):
    """Recursively compare two JSON values; append diff strings to `diffs`."""
    if type(va) != type(vb):
        # Allow int/float interop
        if isinstance(va, (int, float)) and isinstance(vb, (int, float)):
            if not floats_close(va, vb, tol):
                diffs.append(f"  {path}: {va!r} vs {vb!r}")
        else:
            diffs.append(f"  {path}: type {type(va).__name__} vs {type(vb).__name__}: {va!r} vs {vb!r}")
        return
    if isinstance(va, dict):
        all_keys = sorted(set(va) | set(vb))
        for k in all_keys:
            if k not in va:
                diffs.append(f"  {path}.{k}: missing in A, present in B ({vb[k]!r})")
            elif k not in vb:
                diffs.append(f"  {path}.{k}: present in A ({va[k]!r}), missing in B")
            else:
                diff_values(f"{path}.{k}", va[k], vb[k], tol, diffs)
    elif isinstance(va, list):
        if len(va) != len(vb):
            diffs.append(f"  {path}: list length {len(va)} vs {len(vb)}")
        for i, (ea, eb) in enumerate(zip(va, vb)):
            diff_values(f"{path}[{i}]", ea, eb, tol, diffs)
    elif isinstance(va, (int, float)):
        if not floats_close(va, vb, tol):
            diffs.append(f"  {path}: {va!r} vs {vb!r}")
    else:
        if va != vb:
            diffs.append(f"  {path}: {va!r} vs {vb!r}")


def main():
    args = parse_args()
    skip_types = set(args.skip_type)
    skip_names = set(args.skip_name)

    raw_a = load_json(args.file_a)
    raw_b = load_json(args.file_b)

    if not isinstance(raw_a, list):
        raw_a = [raw_a]
    if not isinstance(raw_b, list):
        raw_b = [raw_b]

    idx_a = index_by_key(raw_a)
    idx_b = index_by_key(raw_b)

    all_ok = True
    n_pass = n_fail = n_skip = n_only_a = n_only_b = 0

    # Find keys only in A or only in B
    for key in sorted(idx_a):
        t, name = key
        if t in skip_types or name in skip_names:
            n_skip += 1
            continue
        if key not in idx_b:
            print(f"ONLY_A  {t}:{name}")
            n_only_a += 1

    for key in sorted(idx_b):
        t, name = key
        if t in skip_types or name in skip_names:
            continue
        if key not in idx_a:
            print(f"ONLY_B  {t}:{name}")
            n_only_b += 1

    # Compare matched objects
    for key in sorted(idx_a):
        t, name = key
        if t in skip_types or name in skip_names:
            n_skip += 1
            continue
        if key not in idx_b:
            continue
        ca = idx_a[key]
        cb = idx_b[key]
        diffs = []
        diff_values("data", ca.get("data", {}), cb.get("data", {}), args.float_tol, diffs)
        if diffs:
            print(f"DIFF    {t}:{name}")
            for d in diffs:
                print(d)
            n_fail += 1
            all_ok = False
        else:
            if args.show_match:
                print(f"PASS    {t}:{name}")
            n_pass += 1

    print()
    print(f"RESULT: {'PASS' if all_ok else 'FAIL'}")
    print(f"  matched={n_pass} diff={n_fail} only_a={n_only_a} only_b={n_only_b} skipped={n_skip}")
    print(f"  A: {args.file_a}")
    print(f"  B: {args.file_b}")

    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
