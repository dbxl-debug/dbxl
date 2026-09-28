#!/usr/bin/env python3
"""m2_compare.py GOTDIR REFDIR OUTDIR [SCALE]

Compare the milestone 2 scenario captures (full-screen, from scenario.py)
with the xldb references (cropped to the frame).  Each step masks only what
dbxl cannot show yet (it has no program) or deliberately shows differently.
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
CALLERS = "530,101,206,14"        # xldb's selected __start() frame
ARROW = "7,392,38,18"             # xldb's execution arrow in Source
COMMON = [CALLERS, ARROW]
EXTRA = {
    # Source maximized: its arrow moves to the top of the frame.
    "m2-05-source-maximized": ["6,17,38,20"],
    # "Exit from xldb?" vs "Exit from dbxl?".
    "m2-13-exit-dialog": ["458,420,36,13"],
    # Program-dependent contents.
    "m2-30-open-threads": ["7,170,784,13"],
    "m2-30-open-registers": ["745,394,207,427"],
    "m2-30-open-storage": ["8,394,594,427"],
}

failed = 0
for name in sorted(f[:-4] for f in os.listdir(ref) if f.endswith(".png")):
    masks = COMMON + EXTRA.get(name, [])
    args = [sys.executable, xdiff, os.path.join(got, name + ".png"),
            os.path.join(ref, name + ".png"),
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
print(f"{failed} of {len(os.listdir(ref))} steps differ" if failed else
      "all steps match")
sys.exit(1 if failed else 0)
