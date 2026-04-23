#!/usr/bin/env python3
"""
compare_frames_npz.py  [--max-diff N] [--max-ch-diff N] FILE_A FILE_B

Compare two WCT FrameFileSink NPZ outputs.

Used by phlex_sim_sigproc_compare to validate that the shared-instance and
independent-instance two-island workflows produce equivalent results when the
same data files are used.

Exit codes:
  0  all checks passed
  1  one or more checks failed
  2  usage error or file not found

Expected NPZ keys (WCT FrameFileSink format):
  channels_TAG  int32   shape (nch,)           channel IDs
  frame_TAG     float32 shape (nch, nticks)    waveform data
  tickinfo_TAG  float64 shape (3,)             [t0, tick_period, something]

Comparison strategy:
  - tickinfo arrays: must be exactly equal.
  - channels arrays: channel count may differ by at most --max-ch-diff (default 1).
    The waveform comparison uses only the channels present in BOTH files.
  - frame arrays (common channels): |diff| <= --max-diff (default 2) for every sample.

The tolerance exists because shared-instance mode calls configure() twice on
service components (AnodePlane, WireSchemaFile, FieldResponse) due to WCT's
global NamedFactory deduplication.  The re-configure with identical parameters
is functionally a no-op but may produce 1–2 ADC rounding differences in
OmnibusSigProc's Wiener-filter output.  Additionally, sparse=true in
OmnibusSigProc means that near-threshold channels may appear or disappear
between runs (--max-ch-diff covers this).
"""

import sys
import argparse
import numpy as np


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("file_a")
    p.add_argument("file_b")
    p.add_argument("--max-diff", type=float, default=2.0,
                   help="maximum allowed |diff| per sample in frame arrays (default 2)")
    p.add_argument("--max-ch-diff", type=int, default=1,
                   help="maximum allowed difference in channel count (default 1)")
    return p.parse_args()


def find_keys(npz, prefix):
    return [k for k in npz.keys() if k.startswith(prefix)]


def main():
    args = parse_args()

    try:
        a = np.load(args.file_a)
    except Exception as e:
        print(f"ERROR loading {args.file_a}: {e}", file=sys.stderr)
        sys.exit(2)
    try:
        b = np.load(args.file_b)
    except Exception as e:
        print(f"ERROR loading {args.file_b}: {e}", file=sys.stderr)
        sys.exit(2)

    all_ok = True

    # Collect matching (tag, prefix) pairs
    tags_a = {k.split("_", 1)[1] for k in a.keys()}
    tags_b = {k.split("_", 1)[1] for k in b.keys()}
    if tags_a != tags_b:
        print(f"FAIL  frame tags differ: {sorted(tags_a)} vs {sorted(tags_b)}")
        all_ok = False
    common_tags = tags_a & tags_b

    for tag in sorted(common_tags):
        ch_key   = f"channels_{tag}"
        fr_key   = f"frame_{tag}"
        ti_key   = f"tickinfo_{tag}"

        # --- tickinfo: must match exactly ---
        if ti_key in a and ti_key in b:
            if np.array_equal(a[ti_key], b[ti_key]):
                print(f"PASS  {ti_key:<35s}  {a[ti_key].shape}  {a[ti_key].dtype}")
            else:
                print(f"FAIL  {ti_key:<35s}  exact mismatch: "
                      f"{a[ti_key]} vs {b[ti_key]}")
                all_ok = False

        # --- channels: count may differ by at most max_ch_diff ---
        ch_a = a[ch_key] if ch_key in a else None
        ch_b = b[ch_key] if ch_key in b else None
        if ch_a is None or ch_b is None:
            print(f"FAIL  {ch_key}: missing in one file")
            all_ok = False
            continue

        ch_diff = abs(len(ch_a) - len(ch_b))
        if ch_diff <= args.max_ch_diff:
            print(f"PASS  {ch_key:<35s}  {len(ch_a)} vs {len(ch_b)} channels  "
                  f"(diff={ch_diff} ≤ {args.max_ch_diff})")
        else:
            print(f"FAIL  {ch_key:<35s}  {len(ch_a)} vs {len(ch_b)} channels  "
                  f"(diff={ch_diff} > {args.max_ch_diff})")
            all_ok = False

        # --- frame: compare common channels ---
        if fr_key not in a or fr_key not in b:
            print(f"FAIL  {fr_key}: missing in one file")
            all_ok = False
            continue

        fr_a = a[fr_key]   # shape (nch_a, nticks)
        fr_b = b[fr_key]   # shape (nch_b, nticks)

        common_ch = sorted(set(ch_a.tolist()) & set(ch_b.tolist()))
        n_common = len(common_ch)
        n_only_a = len(set(ch_a.tolist()) - set(ch_b.tolist()))
        n_only_b = len(set(ch_b.tolist()) - set(ch_a.tolist()))

        # Build index maps for the common channels
        idx_a = {c: i for i, c in enumerate(ch_a.tolist())}
        idx_b = {c: i for i, c in enumerate(ch_b.tolist())}
        rows_a = np.array([idx_a[c] for c in common_ch])
        rows_b = np.array([idx_b[c] for c in common_ch])

        sub_a = fr_a[rows_a]
        sub_b = fr_b[rows_b]
        diff = sub_a.astype(np.float64) - sub_b.astype(np.float64)

        n_diff   = int(np.count_nonzero(diff))
        max_diff = float(np.max(np.abs(diff))) if n_diff > 0 else 0.0
        pct_diff = 100.0 * n_diff / diff.size if diff.size > 0 else 0.0

        channel_note = f"{n_common} common"
        if n_only_a or n_only_b:
            channel_note += f"  (+{n_only_a}/-{n_only_b} exclusive)"

        if max_diff <= args.max_diff:
            print(f"PASS  {fr_key:<35s}  max={max_diff:.3g}  "
                  f"n={n_diff} ({pct_diff:.3f}%)  "
                  f"{channel_note}  {fr_a.shape[1]} ticks  {fr_a.dtype}")
        else:
            print(f"FAIL  {fr_key:<35s}  max={max_diff:.3g} > {args.max_diff}  "
                  f"n={n_diff} ({pct_diff:.3f}%)  "
                  f"{channel_note}  {fr_a.shape[1]} ticks  {fr_a.dtype}")
            all_ok = False

    print()
    if all_ok:
        print(f"RESULT: PASS  (max-diff={args.max_diff}, max-ch-diff={args.max_ch_diff})")
        print(f"  {args.file_a}")
        print(f"  {args.file_b}")
    else:
        print(f"RESULT: FAIL  (max-diff={args.max_diff}, max-ch-diff={args.max_ch_diff})")
        print(f"  {args.file_a}")
        print(f"  {args.file_b}")

    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
