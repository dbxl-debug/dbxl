#!/usr/bin/env python3
"""scenario_m5.py DISPLAY OUTDIR [--wait S] [--scale N] [--xldb]

Milestone 5a scenario (recon pass 14), debugging tests/progs/rich: the
Breakpoints window and its actions, a conditional breakpoint, then
Disassembly, Registers, Storage, Threads, Subprograms and Files.

--xldb: the extra Continue xldb needs on the emulator's first run, and
Storage words at xldb's 32-bit columns (dbxl's addresses are 64-bit, so
its words start 8 columns further right).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scenario import Driver, command  # noqa: E402

SOURCE_TOP = 454 + 13


def source_y(line, top_line):
    return SOURCE_TOP + (line - top_line) * 13 + 6


def bp_row(k):                       # Breakpoints entries
    return 296 + 13 * k


def bp_item(k):                      # Select Breakpoint Action items
    return 380 + 19 * k


def item_y(k):                       # variable menu items (default spot)
    return 237 + 13 * k


def steps(dr, xldb):
    word_col = 11 if xldb else 19    # Storage: first word's column
    word_dx = 64 if not xldb else 0  # menus and fields follow the word

    dr.shot("m5-01-start")
    dr.click(180, source_y(44, 24))
    dr.click(180, source_y(47, 24))
    dr.click(776, 113)                                 # Breakpoints chip
    dr.shot("m5-02-breakpoints")
    dr.click(450, bp_row(0))
    dr.shot("m5-03-action-menu")
    dr.click(790, bp_item(1))                          # Disable
    dr.shot("m5-04-disabled")
    dr.click(450, bp_row(0))
    dr.click(790, bp_item(2))                          # Enable
    dr.shot("m5-05-enabled")
    dr.click(450, bp_row(0))
    dr.click(790, bp_item(4))                          # Disable all
    dr.shot("m5-06-disable-all")
    dr.click(450, bp_row(0))
    dr.click(790, bp_item(5))                          # Enable all
    dr.shot("m5-07-enable-all")
    dr.click(450, bp_row(1))
    dr.click(790, bp_item(0))                          # Clear (line 47)
    dr.shot("m5-08-cleared-one")
    dr.click(450, bp_row(0))
    dr.click(790, bp_item(3))                          # Clear all
    dr.shot("m5-09-clear-all-dialog")
    dr.click(672, 486)                                 # yes
    dr.shot("m5-10-cleared-all")

    # A conditional breakpoint on a, at line 47
    dr.click(180, source_y(47, 29))
    if xldb:
        dr.click(*command(0))
    dr.click(*command(0))
    dr.shot("m5-11-at-47")
    dr.click(60, 113)
    dr.click(420, item_y(12))                          # Breakpoint
    dr.shot("m5-12-trigger-dialog")
    dr.key("Return")
    dr.shot("m5-13-conditional")
    dr.click(*command(1))                              # Next
    dr.shot("m5-14-next")
    dr.click(*command(7))                              # Restart
    dr.click(*command(0))                              # Continue
    dr.shot("m5-15-condition-hit")

    # Disassembly: a breakpoint on main's first instruction, Machine step
    dr.click(632, 140)
    dr.shot("m5-20-disassembly")
    dr.click(400, 473)
    dr.shot("m5-21-disassembly-breakpoint")
    dr.click(930, 220)                                 # Machine step
    dr.shot("m5-22-machine-step")

    # Registers: the intregister menu, signed
    dr.click(776, 140)
    dr.shot("m5-30-registers")
    dr.click(800, 473)
    dr.shot("m5-31-register-menu")
    dr.click(420, item_y(2))
    dr.shot("m5-32-register-signed")

    # Storage: the word menu, Edit (then Escape), Storage view from Locals
    dr.click(920, 140)
    dr.shot("m5-40-storage")
    wx = 41 + word_col * 8 + 16
    dr.click(wx, 512)
    dr.shot("m5-41-storage-menu")
    dr.click(181 + word_dx, 529)                       # Edit
    dr.shot("m5-42-storage-edit")
    dr.move(wx, 511)
    dr.key("Escape")
    dr.shot("m5-43-storage-edit-escape")
    dr.click(60, 113)
    dr.click(420, item_y(15))                          # a: Storage view
    dr.shot("m5-44-storage-view")

    # Threads, Subprograms, Files
    dr.click(920, 113)
    dr.shot("m5-50-threads")
    dr.click(632, 113)
    dr.click(100, 232)                                 # area()
    dr.shot("m5-51-subprogram")
    dr.click(935, 86)
    dr.click(720, 355)                                 # rich.c
    dr.shot("m5-52-file")
    dr.move(640, 512)
    dr.shot("m5-53-final")


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
