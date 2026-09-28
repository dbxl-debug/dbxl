#!/usr/bin/env python3
"""m4_compare.py GOTDIR REFDIR OUTDIR [SCALE]

Compare the milestone 4 scenario captures (scenario_m4.py, debugging
tests/progs/rich) with the xldb references in REFDIR/m4 (frame crops).
Exit status 0 when every step matches.
"""
import os
import subprocess
import sys

got, ref, out = sys.argv[1:4]
n = int(sys.argv[4]) if len(sys.argv) > 4 else 1
here = os.path.dirname(os.path.abspath(__file__))
xdiff = os.path.join(here, "..", "..", "tools", "xdiff.py")

# Masks in frame-crop coordinates (root x-33, y-73).
CALLERS_TAIL = "530,114,210,40"   # xldb lists __start() below main()
LOCALS = "5,19,500,340"           # values that depend on the stack contents
EXTRA = {
    # Stopped at area's first instruction: its locals aren't set yet, and
    # xldb misreads main's locals from there (recon pass 13).
    "m4-27-in-area": [LOCALS],
    "m4-28-select-main": [LOCALS],
    "m4-29-select-area": [LOCALS],
    "m4-30-final": [LOCALS],
}

d = os.path.join(ref, "m4")
failed = total = 0
for name in sorted(f[:-4] for f in os.listdir(d) if f.endswith(".png")):
    total += 1
    masks = [CALLERS_TAIL] + EXTRA.get(name, [])
    args = [sys.executable, xdiff, os.path.join(got, name + ".png"),
            os.path.join(d, name + ".png"),
            "--crop", f"{33 * n},{73 * n},{959 * n},{848 * n}",
            "--ref-scale", str(n),
            "--out", os.path.join(out, name + "-diff.png")]
    for m in masks:
        args += ["--mask", m]
    r = subprocess.run(args, capture_output=True, text=True)
    status = "ok" if r.returncode == 0 else "FAIL"
    print(f"  {name:32s} {status}")
    if r.returncode != 0:
        failed += 1
        print("    " + r.stdout.strip().replace("\n", "\n    "))
print(f"{failed} of {total} steps differ" if failed else "all steps match")
sys.exit(1 if failed else 0)
