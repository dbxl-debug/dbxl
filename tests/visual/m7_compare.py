#!/usr/bin/env python3
"""m7_compare.py GOTDIR REFDIR OUTDIR [SCALE]

Compare the milestone 6 scenario (scenario_m7.py) with the xldb references
in REFDIR/m7 (frame crops).  Masked: area's uninitialised locals (garbage
on both machines), and the end message: xldb's program dies of the
emulator's SIGSEGV in exit() (`"./rich" terminated. Termination code is:
-1.`) where dbxl's exits normally (`Program exited with return code 198.`).

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
LOCALS_VALUES = "30,34,90,42"      # w, h: stack garbage
MESSAGE = "3,826,953,18"           # the end message


def masks_for(name):
    step = int(name.split("-")[1])
    m = []
    if step <= 4:
        m.append(LOCALS_VALUES)
    if name in ("m7-05-terminated", "m7-10-source-click"):
        m.append(MESSAGE)
    return m

SUB = "m7"

failed = total = 0
d = os.path.join(ref, SUB)
for name in sorted(f[:-4] for f in os.listdir(d) if f.endswith(".png")):
    total += 1
    args = [sys.executable, xdiff, os.path.join(got, name + ".png"),
            os.path.join(d, name + ".png"),
            "--crop", f"{33 * n},{73 * n},{959 * n},{848 * n}",
            "--ref-scale", str(n),
            "--out", os.path.join(out, name + "-diff.png")]
    for m in masks_for(name):
        args += ["--mask", m]
    r = subprocess.run(args, capture_output=True, text=True)
    status = "ok" if r.returncode == 0 else "FAIL"
    print(f"  {name:32s} {status}")
    if r.returncode != 0:
        failed += 1
        print("    " + r.stdout.strip().replace("\n", "\n    "))
print(f"{failed} of {total} steps differ" if failed else "all steps match")
sys.exit(1 if failed else 0)
