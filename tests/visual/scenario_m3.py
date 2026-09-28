#!/usr/bin/env python3
"""scenario_m3.py DISPLAY OUTDIR [--wait S] [--scale N] [--xldb]

Milestone 3 scenario: debugging tests/progs/test.c (the recon test
program) with the default layout, started with its Source file in the
current directory.  Captures after each step, like scenario.py.

--xldb adds one extra Continue before the first real one: on the AIX
emulator xldb's first Continue always stops with "signal 19: SIGCONT"
(recon pass 2), which dbxl does not reproduce.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scenario import Driver, command  # noqa: E402

SOURCE_TOP = 454 + 13          # root y of Source's first text row


def line_y(line, top_line=1):
    return SOURCE_TOP + (line - top_line) * 13 + 6


def steps(dr, xldb):
    dr.shot("m3-01-startup")                       # stopped at main's {
    dr.click(180, line_y(17))                      # breakpoint on line 17
    dr.shot("m3-02-breakpoint-set")
    if xldb:
        dr.click(*command(0))                      # the SIGCONT stop
    dr.click(*command(0))                          # Continue
    dr.shot("m3-03-breakpoint-hit")
    dr.click(*command(2))                          # Step (into add)
    dr.shot("m3-04-step-into")
    dr.click(*command(1))                          # Next
    dr.shot("m3-05-next")
    dr.click(*command(4))                          # Return
    dr.shot("m3-06-return")
    dr.click(*command(1))                          # Next
    dr.shot("m3-07-next")
    dr.click(*command(0))                          # Continue: line 17 again
    dr.shot("m3-08-continue")
    dr.move(640, 512)
    dr.click(180, dr.last_line_y)                  # clear the breakpoint
    dr.shot("m3-09-breakpoint-cleared")
    dr.click(*command(0))                          # Continue: runs to exit
    dr.shot("m3-10-exited")
    dr.click(*command(7))                          # Restart
    dr.shot("m3-11-restarted")
    dr.click(*command(9))                          # Breakpoint dialog
    dr.move(500, 543)
    dr.type("add")
    dr.key("Return")
    dr.shot("m3-12-breakpoint-add")
    dr.click(*command(0))                          # Continue into add
    dr.shot("m3-13-in-add")
    dr.click(*command(3))                          # Machine step
    dr.shot("m3-14-machine-step")
    dr.move(640, 512)
    dr.shot("m3-15-final")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("display")
    ap.add_argument("outdir")
    ap.add_argument("--wait", type=float, default=0.5)
    ap.add_argument("--scale", type=int, default=1)
    ap.add_argument("--xldb", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    dr = Driver(a.display, a.outdir, a.wait, a.scale)
    # After a stop the Source view is centred on the current line; the
    # clear-breakpoint click needs line 17's position in that view.
    dr.last_line_y = line_y(17, 17 - 15)
    dr.move(640, 512)
    steps(dr, a.xldb)


if __name__ == "__main__":
    main()
