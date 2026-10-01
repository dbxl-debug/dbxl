#!/usr/bin/env python3
"""rec.py LOG OUTDIR ACTION...  - drive xldb on :42 and record the actions.

ACTIONS (root coordinates at scale 1):
  click X Y [BUTTON]   drag X1 Y1 X2 Y2 BUTTON
  drag3 X1 Y1 X2 Y2 X3 Y3 BUTTON   move X Y   key KEYSYM   type TEXT   wait S   shot NAME

Every action is appended to LOG (one JSON list per line) so the session
can be replayed against dbxl with replay.py.  Shots go to OUTDIR/NAME.png.
"""
import json
import os
import sys
import time

from PIL import ImageGrab
from Xlib import X, XK, display
from Xlib.ext import xtest

DISP = os.environ.get("REC_DISPLAY", ":42")
WAIT = float(os.environ.get("REC_WAIT", "2"))


class Driver:
    def __init__(self, disp, outdir, wait, scale=1):
        self.d = display.Display(disp)
        self.disp = disp
        self.outdir = outdir
        self.wait = wait
        self.n = scale

    def move(self, x, y):
        n = self.n
        xtest.fake_input(self.d, X.MotionNotify, x=x * n + n // 2, y=y * n + n // 2)
        self.d.sync()
        time.sleep(0.2)

    def click(self, x, y, b=1):
        self.move(x, y)
        xtest.fake_input(self.d, X.ButtonPress, b)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.ButtonRelease, b)
        self.d.sync()
        time.sleep(self.wait)

    def drag(self, x1, y1, x2, y2, b=3):
        self.move(x1, y1)
        xtest.fake_input(self.d, X.ButtonPress, b)
        self.d.sync()
        time.sleep(0.2)
        steps = 10
        for i in range(1, steps + 1):
            self.move(x1 + (x2 - x1) * i // steps, y1 + (y2 - y1) * i // steps)
        xtest.fake_input(self.d, X.ButtonRelease, b)
        self.d.sync()
        time.sleep(self.wait)

    def drag3(self, x1, y1, x2, y2, x3, y3, b=3):
        self.move(x1, y1)
        xtest.fake_input(self.d, X.ButtonPress, b)
        self.d.sync()
        time.sleep(0.2)
        for (ax, ay, bx, by) in ((x1, y1, x2, y2), (x2, y2, x3, y3)):
            for i in range(1, 11):
                self.move(ax + (bx - ax) * i // 10, ay + (by - ay) * i // 10)
        xtest.fake_input(self.d, X.ButtonRelease, b)
        self.d.sync()
        time.sleep(self.wait)

    def key(self, name):
        shifted = {'colon', 'question', 'exclam', 'plus', 'underscore',
                   'parenleft', 'parenright', 'less', 'greater', 'quotedbl',
                   'asterisk', 'braceleft', 'braceright'}
        code = self.d.keysym_to_keycode(XK.string_to_keysym(name))
        sc = self.d.keysym_to_keycode(XK.string_to_keysym('Shift_L'))
        if name in shifted:
            xtest.fake_input(self.d, X.KeyPress, sc)
        xtest.fake_input(self.d, X.KeyPress, code)
        xtest.fake_input(self.d, X.KeyRelease, code)
        if name in shifted:
            xtest.fake_input(self.d, X.KeyRelease, sc)
        self.d.sync()
        time.sleep(0.3)

    def type(self, text):
        names = {':': 'colon', '/': 'slash', '.': 'period', ' ': 'space',
                 '-': 'minus', '+': 'plus', "'": 'apostrophe', '"': 'quotedbl',
                 '\\': 'backslash', '_': 'underscore', ',': 'comma'}
        for ch in text:
            self.key(names.get(ch, ch))
        time.sleep(self.wait)

    def shot(self, name):
        time.sleep(self.wait)
        ImageGrab.grab(xdisplay=self.disp).save(
            os.path.join(self.outdir, name + ".png"))
        print("captured", name, flush=True)


def run(dr, act):
    op, args = act[0], act[1:]
    if op == "click":
        dr.click(int(args[0]), int(args[1]), int(args[2]) if len(args) > 2 else 1)
    elif op == "move":
        dr.move(int(args[0]), int(args[1]))
    elif op == "drag3":
        dr.drag3(*[int(a) for a in args])
    elif op == "drag":
        dr.drag(*[int(a) for a in args])
    elif op == "key":
        dr.key(args[0])
    elif op == "type":
        dr.type(args[0])
    elif op == "wait":
        time.sleep(float(args[0]))
    elif op == "shot":
        dr.shot(args[0])
    else:
        raise SystemExit(f"unknown action {op}")


def parse(argv):
    acts, i = [], 0
    arity = {"click": None, "drag": 5, "drag3": 7, "move": 2, "key": 1, "type": 1, "wait": 1, "shot": 1}
    while i < len(argv):
        op = argv[i]
        if op not in arity:
            raise SystemExit(f"unknown action {op}")
        if op == "click":
            n = 3 if i + 3 < len(argv) and argv[i + 3].isdigit() else 2
        else:
            n = arity[op]
        acts.append([op] + argv[i + 1:i + 1 + n])
        i += 1 + n
    return acts


if __name__ == "__main__":
    log, outdir = sys.argv[1:3]
    os.makedirs(outdir, exist_ok=True)
    dr = Driver(DISP, outdir, WAIT)
    with open(log, "a") as f:
        for act in parse(sys.argv[3:]):
            run(dr, act)
            f.write(json.dumps(act) + "\n")
            f.flush()
