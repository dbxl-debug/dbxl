#!/usr/bin/env python3
"""scenario_m7.py DISPLAY OUTDIR [--wait S] [--scale N]

Milestone 6 scenario (recon pass 17), debugging tests/progs/rich started as

    rich-debugger -i segv -i 19 -r area -c 2 -e 2 ./rich

-r stops in area; -c 2 cuts Callers to `area()` and `[...]`; -e 2 cuts
arrays (`[ +1 +2  ...]`, then `...]` on its own line).  Continue then ends
the program (xldb: the emulator's SIGSEGV in exit(), passed on by -i segv;
dbxl: a normal exit), leaving the terminated display, where the run
commands only say they can't continue, Subprograms still shows source and
a Source click brings the terminated display back.  -i 19 drops the
emulator's first-run SIGCONT stop.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scenario import Driver, command  # noqa: E402

ARGS = ["-i", "segv", "-i", "19", "-r", "area", "-c", "2", "-e", "2", "./rich"]


def item_y(k):                        # variable menu item k (default spot)
    return 237 + 13 * k


def steps(dr):
    dr.shot("m7-01-start")
    dr.click(720, 87)                                  # Globals
    dr.click(150, 337)                                 # counts
    dr.click(420, item_y(1))                           # more
    dr.shot("m7-02-counts-cut")
    dr.click(150, 337)
    dr.click(420, item_y(1))                           # more: vertical
    dr.shot("m7-03-counts-vertical")
    dr.click(150, 259)                                 # box
    dr.click(420, item_y(1))                           # more
    dr.shot("m7-04-box")
    dr.move(950, 300)                                  # raise Commands
    dr.click(*command(0))                              # Continue: the end
    dr.shot("m7-05-terminated")
    dr.click(*command(0))                              # Continue again
    dr.shot("m7-06-cannot-continue")
    dr.click(*command(5))                              # Signal
    dr.shot("m7-07-signal")
    dr.click(632, 113)                                 # Subprograms
    dr.shot("m7-08-subprograms")
    dr.click(100, 232)                                 # area()
    dr.shot("m7-09-area-source")
    dr.click(180, 707)                                 # a Source line
    dr.shot("m7-10-source-click")


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
