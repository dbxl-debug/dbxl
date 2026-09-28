#!/usr/bin/env python3
"""m6_compare.py GOTDIR REFDIR OUTDIR [SCALE]

Compare the milestone 5b scenario (scenario_m6.py) with the xldb
references in REFDIR/m6 (frame crops).  Masked: the __start() line xldb
lists in Callers, the default breakpoints file name (.xldb.rich vs
.dbxl.rich) and the Help text, which is dbxl's own (its title bar is
compared).

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
CALLERS_TAIL = "530,114,210,40"    # xldb lists __start() below main()
BP_FILE = "5,462,130,16"           # the prompt's field: .xldb.rich
HELP_TEXT = "3,16,603,741"         # Help's rows and scroll bars


def masks_for(name):
    m = [CALLERS_TAIL]
    if name in ("m6-03-save-breakpoints", "m6-05-load-breakpoints"):
        m.append(BP_FILE)
    if name in ("m6-30-help", "m6-31-help-link", "m6-32-help-return"):
        m.append(HELP_TEXT)
    return m


failed = total = 0
d = os.path.join(ref, "m6")
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
