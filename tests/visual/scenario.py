#!/usr/bin/env python3
"""scenario.py DISPLAY OUTDIR [--wait SECONDS] [--scale N]

Drive a running xldb or dbxl (default layout, no program or /usr/bin/bsh)
through the milestone 2 UI scenario and save a full-screen capture after
each step as OUTDIR/NAME.png.  Coordinates are root coordinates at scale 1
with the frame at its default position (+33+73); --scale N multiplies
them (to drive dbxl -scale N, whose captures should equal the references
enlarged NxN).

The same script produced the reference captures from the real xldb, so a
dbxl run can be compared step by step (see run.sh).
"""
import argparse
import os
import time

from PIL import ImageGrab
from Xlib import X, XK, display
from Xlib.ext import xtest

FRAME_X, FRAME_Y = 35, 75          # frame inside origin (root), border 2


def pane_outer(fx, fy):
    """Root position of a pane's outer (border) corner from frame coords."""
    return FRAME_X + fx, FRAME_Y + fy


class Driver:
    def __init__(self, disp, outdir, wait, scale=1):
        self.n = scale
        self.d = display.Display(disp)
        self.disp = disp
        self.outdir = outdir
        self.wait = wait

    def move(self, x, y):
        # Logical root position -> the centre of the enlarged pixel.
        x, y = x * self.n + self.n // 2, y * self.n + self.n // 2
        xtest.fake_input(self.d, X.MotionNotify, x=x, y=y)
        self.d.sync()
        time.sleep(0.2)

    def click(self, x, y, button=1):
        self.move(x, y)
        xtest.fake_input(self.d, X.ButtonPress, button)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.ButtonRelease, button)
        self.d.sync()
        time.sleep(self.wait)

    def key(self, name):
        code = self.d.keysym_to_keycode(XK.string_to_keysym(name))
        shift = name in ('colon', 'question', 'exclam')
        sc = self.d.keysym_to_keycode(XK.string_to_keysym('Shift_L'))
        if shift:
            xtest.fake_input(self.d, X.KeyPress, sc)
        xtest.fake_input(self.d, X.KeyPress, code)
        xtest.fake_input(self.d, X.KeyRelease, code)
        if shift:
            xtest.fake_input(self.d, X.KeyRelease, sc)
        self.d.sync()
        time.sleep(0.3)

    def type(self, text):
        names = {':': 'colon', '/': 'slash', '.': 'period', ' ': 'space',
                 '-': 'minus'}
        for ch in text:
            self.key(names.get(ch, ch))
        time.sleep(self.wait)

    def shot(self, name):
        time.sleep(self.wait)
        ImageGrab.grab(xdisplay=self.disp).save(os.path.join(self.outdir, name + ".png"))
        print("captured", name, flush=True)


# Window Control menu: opens at the pane's outer top-left; item i (0 = the
# "Window Control" title) is centred at y = top + 9 + 19*i, x = left + 90.
WC = {"Restore": 1, "Move": 2, "Size": 3, "Minimize": 4, "Maximize": 5,
      "Lower": 6, "Horizontal scroll bars": 7, "Vertical scroll bars": 8,
      "Save Window": 9}


def wc_item(fx, fy, item):
    x, y = pane_outer(fx, fy)
    return x + 90, y + 9 + 19 * WC[item]


def title(fx, fy, fw):
    x, y = pane_outer(fx, fy)
    return x + fw // 2, y + 8


LOCALS = (3, 4, 516)
SOURCE = (3, 377, 947)
COMMANDS_ROW0 = (812, 181)          # "Continue"; rows are 13px apart


def command(row):
    return COMMANDS_ROW0[0] - 16, COMMANDS_ROW0[1] + 13 * row


def steps(dr):
    # --- Window Control menu on Locals, dismissed with Escape ------------
    dr.click(*title(*LOCALS))
    dr.shot("m2-01-wc-menu")
    dr.move(100, 150)                              # inside the menu
    dr.key("Escape")
    dr.shot("m2-02-wc-escape")

    # --- Minimize Locals, reopen from its chip ---------------------------
    dr.click(*title(*LOCALS))
    dr.click(*wc_item(3, 4, "Minimize"))
    dr.shot("m2-03-locals-minimized")
    dr.click(*pane_outer(528 + 50, 4 + 6))         # Locals chip
    dr.shot("m2-04-locals-reopened")

    # --- Maximize / Restore Source ----------------------------------------
    dr.click(*title(*SOURCE))
    dr.click(*wc_item(3, 377, "Maximize"))
    dr.shot("m2-05-source-maximized")
    dr.click(*title(0, 0, 957))
    dr.click(*wc_item(0, 0, "Restore"))
    dr.shot("m2-06-source-restored")

    # --- Horizontal scroll bar on Locals -----------------------------------
    dr.click(*title(*LOCALS))
    dr.click(*wc_item(3, 4, "Horizontal scroll bars"))
    dr.shot("m2-07-locals-hsb")
    dr.click(*title(*LOCALS))
    dr.click(*wc_item(3, 4, "Horizontal scroll bars"))
    dr.shot("m2-08-locals-hsb-off")

    # --- Options menu, toggle Autoraise twice, Register control ------------
    dr.click(*command(10))
    dr.shot("m2-09-options")
    dr.click(790, 442)                             # Autoraise: yes -> no
    dr.shot("m2-10-options-autoraise")
    dr.click(790, 442)                             # back to yes
    dr.click(790, 499)                             # Register control
    dr.shot("m2-11-register-control")
    dr.move(800, 290)
    dr.key("Escape")
    dr.shot("m2-12-register-control-escape")

    # --- Exit dialog, answer no ------------------------------------------
    dr.click(*command(8))
    dr.shot("m2-13-exit-dialog")
    dr.click(673, 513)                             # no
    dr.shot("m2-14-exit-no")

    # --- Breakpoint dialog: type, then cancel ------------------------------
    dr.click(*command(9))
    dr.shot("m2-15-breakpoint-dialog")
    dr.move(500, 543)
    dr.type("main")
    dr.shot("m2-16-breakpoint-typed")
    dr.click(951, 543)                             # cancel
    dr.shot("m2-17-breakpoint-cancel")

    # --- Save Window dialog on Commands, cancelled ---------------------------
    dr.click(*title(744, 85, 204))
    dr.click(*wc_item(744, 85, "Save Window"))
    dr.shot("m2-18-save-window-dialog")
    dr.click(951, 543)
    dr.shot("m2-19-save-window-cancel")

    # --- Inline input strip in Source -----------------------------------
    dr.move(500, 700)
    dr.key("colon")
    dr.shot("m2-20-strip-colon")
    dr.type("12")
    dr.shot("m2-21-strip-typed")
    dr.key("Escape")
    dr.shot("m2-22-strip-escape")

    # --- Messages bar ---------------------------------------------------
    dr.click(300, 700)                             # a Source line
    dr.shot("m2-23-message")

    # --- Chips: open, capture, minimize again ---------------------------
    for name, (fx, fy, fw, fh), chip in [
            ("globals",     (98, 111, 771, 517), (634 + 50, 4 + 6)),
            ("monitor",     (50, 40, 771, 517),  (740 + 50, 4 + 6)),
            ("files",       (653, 259, 277, 536), (846 + 52, 4 + 6)),
            ("subprograms", (38, 136, 870, 209), (528 + 67, 31 + 6)),
            ("breakpoints", (363, 200, 588, 160), (672 + 67, 31 + 6)),
            ("threads",     (3, 127, 784, 100),  (816 + 67, 31 + 6)),
            ("disassembly", (288, 377, 567, 440), (528 + 67, 58 + 6)),
            ("registers",   (741, 377, 207, 440), (672 + 67, 58 + 6)),
            ("storage",     (4, 377, 594, 440),  (816 + 67, 58 + 6))]:
        dr.click(*pane_outer(*chip))
        dr.shot(f"m2-30-open-{name}")
        dr.click(*title(fx, fy, fw))
        dr.click(*wc_item(fx, fy, "Minimize"))

    dr.move(640, 512)
    dr.shot("m2-40-final")


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
