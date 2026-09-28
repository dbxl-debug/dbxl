#!/usr/bin/env python3
"""m5_compare.py GOTDIR REFDIR OUTDIR [SCALE]

Compare the milestone 5a scenario (scenario_m5.py) with the references:

  REFDIR/m5/       xldb (frame crops).  From step 20 on, what depends on
                   the machine is masked: instructions (POWER vs x86-64),
                   register values and names, memory, the thread id.
  REFDIR/m5-dbxl/  reviewed dbxl captures for the Storage word menu and
                   in-place edit (steps 41-44): with 64-bit addresses the
                   words, and so the menu and field, sit 64px further right
                   than in xldb.

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
DIS = "292,394,567,427"            # Disassembly rows
REGS = "745,394,207,427"           # Registers rows
STORAGE = "8,394,594,427"          # Storage rows
THREAD_ROW = "7,170,784,13"        # the thread's id and state
MESSAGE = "3,826,953,18"           # a message with an address in it
STORAGE_TITLE = "6,379,594,15"     # `Change Block at Address: <addr>`
MACHINE = [DIS, REGS, STORAGE, THREAD_ROW]


def masks_for(name):
    step = int(name.split("-")[1])
    m = [CALLERS_TAIL]
    if step >= 20:
        m += MACHINE
    if name in ("m5-44-storage-view", "m5-50-threads"):
        m.append(MESSAGE)
    # The Storage title holds a 64-bit address in dbxl; xldb also leaves it
    # stale after Escape, where dbxl restores `Storage Pane` (DESIGN.md 11).
    if name in ("m5-42-storage-edit", "m5-43-storage-edit-escape"):
        m.append(STORAGE_TITLE)
    return m


failed = total = 0
for sub in ("m5", "m5-dbxl"):
    d = os.path.join(ref, sub)
    if not os.path.isdir(d):
        continue
    for name in sorted(f[:-4] for f in os.listdir(d) if f.endswith(".png")):
        total += 1
        masks = masks_for(name) if sub == "m5" else [REGS]
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
