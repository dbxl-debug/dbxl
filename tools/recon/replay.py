#!/usr/bin/env python3
"""replay.py LOG DISPLAY OUTDIR [--wait S] [--skip N,...]

Replay a rec.py action log against another display (dbxl), saving the
same shots.  --skip drops the given action numbers (1-based), e.g. the
extra Continue xldb needs on the AIX emulator.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rec import Driver, run  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("log")
ap.add_argument("display")
ap.add_argument("outdir")
ap.add_argument("--wait", type=float, default=1.5)
ap.add_argument("--skip", default="")
a = ap.parse_args()
skip = {int(x) for x in a.skip.split(",") if x}
os.makedirs(a.outdir, exist_ok=True)
dr = Driver(a.display, a.outdir, a.wait)
with open(a.log) as f:
    for i, line in enumerate(f, 1):
        if i in skip:
            continue
        run(dr, json.loads(line))
