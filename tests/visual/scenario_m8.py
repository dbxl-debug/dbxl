#!/usr/bin/env python3
"""scenario_m8.py DISPLAY OUTDIR [--wait S] [--scale N]

Milestone 6 core-file scenario (recon pass 17): `-co ./rich` on a core of
rich killed by SIGSEGV at area's first instruction (line 27).  xldb's core
comes from dbx (`stop in area`, `run`, `cont SIGSEGV`) with sys0 fullcore
and pre430core set; dbxl's from tests/progs/mkcore.py.

The start display, Commands with only Edit, Exit, Options, Help and no Alt
keys, a breakpoint set by a Source click (allowed), Edit refused at once,
a Callers frame, Globals.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scenario import Driver  # noqa: E402

ARGS = ["-co", "./rich"]


def item_y(k):                        # variable menu item k (default spot)
    return 237 + 13 * k


def steps(dr):
    dr.shot("m8-01-start")
    dr.click(180, 733)                                 # line 32: breakpoint
    dr.shot("m8-02-breakpoint")
    dr.move(300, 200)
    dr.click(60, 113)                                  # w
    dr.click(420, item_y(11))                          # Edit
    dr.shot("m8-03-edit-refused")
    dr.click(620, 194)                                 # Callers: main()
    dr.shot("m8-04-main")
    dr.click(720, 87)                                  # Globals
    dr.shot("m8-05-globals")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("display")
    ap.add_argument("outdir")
    ap.add_argument("--wait", type=float, default=0.5)
    ap.add_argument("--scale", type=int, default=1)
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    dr = Driver(a.display, a.outdir, a.wait, a.scale)
    dr.move(640, 512)
    steps(dr)


if __name__ == "__main__":
    main()
