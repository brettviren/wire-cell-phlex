#!/usr/bin/env python3
"""
compare_component.py  [options]  --type TYPE  FILE_A  FILE_B

Compare one WCT component (identified by type) between two JSON arrays.

Unlike compare_wct_configs.py which matches by (type, name), this script
matches by type only — useful when the same component has different instance
names in our configs vs dunereco/toolkit reference configs.

If multiple components of the given type exist in a file, the first is used
unless --index is given.

Exit codes:
  0  all checks passed (within tolerances)
  1  one or more checks failed
  2  usage error, file not found, or component not found

Options:
  --type TYPE        WCT component type to compare (required, e.g. "Drifter")
  --index-a N        Index into FILE_A components of this type (default 0)
  --index-b N        Index into FILE_B components of this type (default 0)
  --skip-field FIELD Dot-separated field path to skip in comparison (repeatable)
  --float-tol TOL    Relative tolerance for float comparisons (default 1e-6)
  --show-match       Print PASS for each matched field
"""

import sys
import json
import argparse


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("file_a")
    p.add_argument("file_b")
    p.add_argument("--type", required=True, metavar="TYPE",
                   help="WCT component type to compare")
    p.add_argument("--index-a", type=int, default=0,
                   help="Which occurrence in FILE_A (0-based, default 0)")
    p.add_argument("--index-b", type=int, default=0,
                   help="Which occurrence in FILE_B (0-based, default 0)")
    p.add_argument("--skip-field", action="append", default=[], metavar="FIELD",
                   help="Dot-separated field to skip (repeatable)")
    p.add_argument("--float-tol", type=float, default=1e-6,
                   help="Relative tolerance for float comparisons (default 1e-6)")
    p.add_argument("--show-match", action="store_true",
                   help="Print PASS lines for matched fields")
    return p.parse_args()


def load_json(path):
    try:
        with open(path) as f:
            data = json.load(f)
    except Exception as e:
        print(f"ERROR loading {path}: {e}", file=sys.stderr)
        sys.exit(2)
    if not isinstance(data, list):
        data = [data]
    return data


def find_by_type(components, wct_type, index):
    matches = [c for c in components if c.get("type") == wct_type]
    if not matches:
        return None
    if index >= len(matches):
        return None
    return matches[index]


def floats_close(a, b, tol):
    if a == b:
        return True
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        denom = max(abs(a), abs(b))
        if denom == 0:
            return True
        return abs(a - b) / denom <= tol
    return False


def diff_values(path, va, vb, tol, skip_fields, diffs, passes, show_match):
    if path in skip_fields:
        return
    if type(va) != type(vb):
        if isinstance(va, (int, float)) and isinstance(vb, (int, float)):
            if not floats_close(va, vb, tol):
                diffs.append(f"  {path}: {va!r} vs {vb!r}")
            elif show_match:
                passes.append(f"  PASS  {path}")
        else:
            diffs.append(f"  {path}: type {type(va).__name__} vs "
                         f"{type(vb).__name__}: {va!r} vs {vb!r}")
        return
    if isinstance(va, dict):
        all_keys = sorted(set(va) | set(vb))
        for k in all_keys:
            child = f"{path}.{k}"
            if child in skip_fields:
                continue
            if k not in va:
                diffs.append(f"  {child}: missing in A, present in B ({vb[k]!r})")
            elif k not in vb:
                diffs.append(f"  {child}: present in A ({va[k]!r}), missing in B")
            else:
                diff_values(child, va[k], vb[k], tol, skip_fields,
                            diffs, passes, show_match)
    elif isinstance(va, list):
        if len(va) != len(vb):
            diffs.append(f"  {path}: list length {len(va)} vs {len(vb)}")
        for i, (ea, eb) in enumerate(zip(va, vb)):
            diff_values(f"{path}[{i}]", ea, eb, tol, skip_fields,
                        diffs, passes, show_match)
    elif isinstance(va, (int, float)):
        if not floats_close(va, vb, tol):
            diffs.append(f"  {path}: {va!r} vs {vb!r}")
        elif show_match:
            passes.append(f"  PASS  {path}")
    else:
        if va != vb:
            diffs.append(f"  {path}: {va!r} vs {vb!r}")
        elif show_match:
            passes.append(f"  PASS  {path}")


def main():
    args = parse_args()
    skip_fields = set(args.skip_field)

    comps_a = load_json(args.file_a)
    comps_b = load_json(args.file_b)

    ca = find_by_type(comps_a, args.type, args.index_a)
    cb = find_by_type(comps_b, args.type, args.index_b)

    if ca is None:
        print(f"ERROR: type '{args.type}' not found in {args.file_a} "
              f"(index {args.index_a})", file=sys.stderr)
        sys.exit(2)
    if cb is None:
        print(f"ERROR: type '{args.type}' not found in {args.file_b} "
              f"(index {args.index_b})", file=sys.stderr)
        sys.exit(2)

    print(f"Comparing {args.type}")
    print(f"  A: {ca.get('name', '(no name)')} from {args.file_a}")
    print(f"  B: {cb.get('name', '(no name)')} from {args.file_b}")

    diffs = []
    passes = []
    diff_values("data", ca.get("data", {}), cb.get("data", {}),
                args.float_tol, skip_fields, diffs, passes, args.show_match)

    if args.show_match:
        for line in passes:
            print(line)

    if diffs:
        for d in diffs:
            print(d)
        print(f"RESULT: FAIL  ({len(diffs)} difference(s))")
        sys.exit(1)
    else:
        print(f"RESULT: PASS")
        sys.exit(0)


if __name__ == "__main__":
    main()
