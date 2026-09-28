#!/usr/bin/env python3
"""scenario_m4.py DISPLAY OUTDIR [--wait S] [--scale N] [--xldb]

Milestone 4 scenario: the data panes and variable menus, debugging
tests/progs/rich (recon pass 13).  Stops at line 47 (after area() returned),
then exercises styles, Edit, detail levels, flatten, wrapping, Globals,
Monitor and Callers frame selection.  Captures after each step.

--xldb adds the extra Continue that xldb on the AIX emulator needs after a
start (it stops first with a signal of its own, recon passes 2 and 12).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scenario import Driver, command  # noqa: E402

SOURCE_TOP = 454 + 13


def source_y(line, top_line):
    return SOURCE_TOP + (line - top_line) * 13 + 6


def locals_y(row):                   # Locals pane rows
    return 94 + 13 * row + 6


def globals_y(row):                  # Globals pane at its default spot
    return 188 + 13 + 13 * row + 6


def item_y(k):                       # variable menu item k at its default spot
    return 237 + 13 * k


MENU_X = 420


def steps(dr, xldb):
    def menu(x, y, k, shot=None):
        dr.click(x, y)
        if shot:
            dr.shot(shot)
        dr.click(MENU_X, item_y(k))

    def cont(first_run):
        # xldb's extra signal stop happens only on the first run.
        if xldb and first_run:
            dr.click(*command(0))
        dr.click(*command(0))

    dr.shot("m4-01-start")                             # stopped at main's {
    dr.click(180, source_y(47, 24))                    # breakpoint on 47
    cont(True)
    dr.shot("m4-02-at-47")

    # int a (row 1): Edit to -5, then hex.  (Edit first: xldb rejects "-5"
    # for a value shown in hex and prompts again.)
    menu(60, locals_y(1), 11, "m4-03-menu-int")       # Edit
    dr.shot("m4-04-edit-dialog")
    for _ in range(12):
        dr.key("BackSpace")
    dr.type("-5")
    dr.key("Return")
    dr.shot("m4-05-a-edited")
    menu(60, locals_y(1), 4)                           # hex
    dr.shot("m4-06-a-hex")
    # char tag (row 2): unsigned; monitor a
    menu(60, locals_y(2), 3, "m4-07-menu-char")
    dr.shot("m4-08-tag-unsigned")
    menu(60, locals_y(1), 13)                          # Monitor on/off

    # pointer sp (row 0): more, more; its name array: more; sp more
    menu(60, locals_y(0), 1, "m4-09-menu-pointer")
    dr.shot("m4-10-sp-more1")
    menu(60, locals_y(0), 1)
    dr.shot("m4-11-sp-more2")
    menu(108, locals_y(0), 1, "m4-12-menu-array")
    dr.shot("m4-13-name-wrapped")
    menu(60, locals_y(0), 1)
    dr.shot("m4-14-sp-vertical")

    # Globals
    dr.click(720, 86)
    dr.shot("m4-15-globals")
    menu(150, globals_y(9), 1, "m4-16-menu-float")    # ratio: decimal
    dr.shot("m4-17-ratio-decimal")
    menu(150, globals_y(9), 10)                        # ratio: Edit
    for _ in range(12):
        dr.key("BackSpace")
    dr.type("1234567")
    dr.key("Return")
    dr.shot("m4-18-ratio-edited")
    menu(150, globals_y(9), 12)                        # ratio: Monitor
    menu(150, globals_y(10), 1)                        # counts: more
    dr.shot("m4-19-counts-inline")
    menu(150, globals_y(4), 3, "m4-20-menu-struct")   # box: flatten
    dr.shot("m4-21-box-flatten")
    menu(150, globals_y(3), 1)                         # square: more, more
    menu(150, globals_y(3), 1)
    dr.shot("m4-22-square-vertical")
    menu(150, globals_y(8), 5)                         # greeting: string
    dr.shot("m4-23-greeting-string")

    # Monitor
    dr.click(826, 86)
    dr.shot("m4-24-monitor")

    # Minimize Monitor and Globals, then select frames in area()
    dr.click(470, 122)
    dr.click(175, 200)
    dr.click(700, 193)                                 # Globals title
    dr.click(223, 271)
    dr.shot("m4-25-minimized")
    dr.click(*command(7))                              # Restart
    dr.shot("m4-26-restarted")
    dr.click(*command(9))                              # Breakpoint: area
    dr.move(500, 543)
    dr.type("area")
    dr.key("Return")
    cont(False)
    dr.shot("m4-27-in-area")
    dr.click(600, 194)                                 # Callers: main()
    dr.shot("m4-28-select-main")
    dr.click(600, 181)                                 # Callers: area()
    dr.shot("m4-29-select-area")
    dr.move(640, 512)
    dr.shot("m4-30-final")


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
