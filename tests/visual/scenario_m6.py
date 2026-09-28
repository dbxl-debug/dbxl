#!/usr/bin/env python3
"""scenario_m6.py DISPLAY OUTDIR [--wait S] [--scale N] [--xldb]

Milestone 5b scenario (recon pass 15), debugging tests/progs/rich: the
Options menu's breakpoint and layout files and Detail per click, Edit input
checks, save/recall, Cast, Select subrange, Function parameter, Breakpoint
all Subprograms, the Source scroll after a trigger, and Help's title
buttons.

--xldb: the extra Continue xldb needs on the emulator's first run.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scenario import Driver, command  # noqa: E402

SOURCE_TOP = 454 + 13
OPTIONS = (796, 311)


def source_y(line, top_line):
    return SOURCE_TOP + (line - top_line) * 13 + 6


def option(k):                        # Options menu item k
    return 845, 328 + 19 * k


def item_y(k):                        # variable menu item k (default spot)
    return 237 + 13 * k


def steps(dr, xldb):
    def menu(x, y, k, shot=None):
        dr.click(x, y)
        if shot:
            dr.shot(shot)
        dr.click(420, item_y(k))

    dr.shot("m6-01-start")
    dr.click(180, source_y(44, 24))
    dr.click(180, source_y(47, 24))
    dr.click(*OPTIONS)
    dr.shot("m6-02-options")
    dr.click(*option(11))                              # Save Breakpoints
    dr.shot("m6-03-save-breakpoints")
    dr.key("Return")
    dr.shot("m6-04-saved")
    dr.click(*OPTIONS)
    dr.click(*option(12))                              # Load Breakpoints
    dr.shot("m6-05-load-breakpoints")
    dr.key("Return")
    dr.shot("m6-06-loaded")
    dr.click(*OPTIONS)
    dr.click(*option(0))                               # Save layout
    dr.shot("m6-07-save-layout")
    dr.key("Return")                                   # no name
    dr.shot("m6-08-layout-unnamed")
    dr.click(*OPTIONS)
    dr.click(*option(0))
    dr.type("/tmp/layout")
    dr.key("Return")
    dr.shot("m6-09-layout-saved")
    dr.click(*OPTIONS)                                 # Detail per click
    dr.click(*option(3), button=3)
    dr.click(*option(3), button=3)
    dr.shot("m6-10-detail-3")
    dr.click(*option(3))
    dr.move(200, 895)
    dr.shot("m6-11-detail-2")

    # Run to line 45 (sp set), then more/less by 2
    if xldb:
        dr.click(*command(0))
    dr.click(*command(0))
    dr.click(*command(1))
    dr.shot("m6-12-at-45")
    menu(60, 100, 1)                                   # sp: more
    dr.shot("m6-13-sp-more-by-2")
    menu(60, 100, 2)                                   # sp: less
    dr.shot("m6-14-sp-less-by-2")

    # a: hex, then Edit with a value that isn't hex
    menu(60, 113, 4)
    menu(60, 113, 11)                                  # Edit
    for _ in range(12):
        dr.key("BackSpace")
    dr.type("-5")
    dr.key("Return")
    dr.shot("m6-15-edit-rejected")
    dr.click(730, 261)                                 # cancel

    # save, Cast, Select subrange, Function parameter on sp
    menu(60, 100, 11)                                  # save
    dr.shot("m6-16-save")
    menu(60, 100, 17)                                  # Cast
    dr.shot("m6-17-cast-menu")
    dr.click(420, 276)                                 # shape
    dr.shot("m6-18-cast")
    menu(60, 100, 15)                                  # Select subrange
    dr.shot("m6-19-subrange-dialog")
    for _ in range(8):
        dr.key("Delete")
    dr.key("bracketleft")
    dr.type("0..1")
    dr.key("bracketright")
    dr.key("Return")
    dr.shot("m6-20-subrange")
    menu(60, 100, 20)                                  # Function parameter
    dr.shot("m6-21-function-parameter")

    # Breakpoint all Subprograms, then the Breakpoints window
    dr.click(*OPTIONS)
    dr.click(*option(14))
    dr.move(200, 895)
    dr.click(776, 113)
    dr.shot("m6-22-breakpoint-all")
    dr.click(694, 283)                                 # minimize it again
    dr.click(488, 360)

    # A trigger on a: Source scrolls the line 4 rows above the bottom
    menu(60, 113, 12)
    dr.key("Return")
    dr.shot("m6-23-trigger-scroll")

    # Help: opens at the index; [index], [return], [exit]
    dr.click(930, 324)
    dr.shot("m6-30-help")
    dr.click(200, 136)                                 # the first link
    dr.shot("m6-31-help-link")
    dr.click(412, 82)                                  # [return]
    dr.shot("m6-32-help-return")
    dr.click(292, 82)                                  # [exit]
    dr.move(640, 512)
    dr.shot("m6-33-help-exit")


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
    dr.move(640, 512)
    steps(dr, a.xldb)


if __name__ == "__main__":
    main()
