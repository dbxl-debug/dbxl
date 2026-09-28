#!/usr/bin/env python3
"""m3_compare.py GOTDIR REFDIR OUTDIR [SCALE]

Compare the milestone 3 scenario captures (scenario_m3.py, debugging
tests/progs/test) with the references:

  REFDIR/m3/       xldb, for the steps where GDB and xldb stop at the
                   same place (everything except program-dependent text);
  REFDIR/m3-dbxl/  dbxl's own reviewed captures, for the steps where GDB
                   and AIX dbx step differently: on AIX, Step at line 17
                   stays on the line (it has three breakpoint addresses),
                   Return from main runs on, and the program dies with
                   SIGSEGV at exit under the emulator.

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
LOCALS = "5,19,500,340"           # variable values: milestone 4
CALLERS_TAIL = "530,114,210,40"   # xldb's __start() below main()
MESSAGE = "3,826,953,18"          # "3 breakpoints set" (3 PowerPC addresses)
COMMON = [LOCALS]
EXTRA = {
    # xldb draws the new stop sign twice, a line apart, without clearing
    # (a stale fragment; dbxl repaints, DESIGN.md 11).
    "m3-02-breakpoint-set": [MESSAGE, "7,600,14,6"],
    "m3-03-breakpoint-hit": [CALLERS_TAIL],
    "m3-08-continue": [CALLERS_TAIL],
    "m3-09-breakpoint-cleared": [CALLERS_TAIL, MESSAGE],
    "m3-14-machine-step": [CALLERS_TAIL],
    "m3-15-final": [CALLERS_TAIL],
}

failed = total = 0
for sub in ("m3", "m3-dbxl"):
    d = os.path.join(ref, sub)
    for name in sorted(f[:-4] for f in os.listdir(d) if f.endswith(".png")):
        total += 1
        masks = COMMON + EXTRA.get(name, [])
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
