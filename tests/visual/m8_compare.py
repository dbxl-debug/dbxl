#!/usr/bin/env python3
"""m8_compare.py GOTDIR REFDIR OUTDIR [SCALE]

Compare the milestone 6 core-file scenario (scenario_m8.py) with the xldb
references in REFDIR/m8 (frame crops).  Masked: the rows xldb lists below
main() in Callers (`0x00000000`, `__start()`); the stop sign xldb draws
offset until its line is redrawn (DESIGN.md 11); main's locals, which
differ between the AIX and Linux cores.

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
CALLERS_TAIL = "530,127,210,40"    # xldb: 0x00000000, __start()
STOP_SIGN = "7,652,16,16"          # line 32's new stop sign
MAIN_LOCALS = "38,21,160,40"       # sp, a, tag in main


def masks_for(name):
    m = [CALLERS_TAIL]
    if name == "m8-02-breakpoint":
        m.append(STOP_SIGN)
    if name in ("m8-04-main", "m8-05-globals"):
        m.append(MAIN_LOCALS)
    return m

SUB = "m8"

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
