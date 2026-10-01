#!/usr/bin/env python3
"""panetext.py TRACE [START_LINE] [TITLE_SUBSTRING...]

Replay xtrace PolyText8 requests and print the text each window shows now:
its title (drawn at y=10) and its rows (drawn at x=0, y=24+13n), trailing
blanks stripped.  Also reports the latest WarpPointer and the map state.
"""
import re
import sys

trace = sys.argv[1]
start = int(sys.argv[2]) if len(sys.argv) > 2 else 1
want = sys.argv[3:]

text = re.compile(r"PolyText8 drawable=(0x[0-9a-f]+) gc=(0x[0-9a-f]+) x=(-?\d+) y=(-?\d+) "
                  r"texts=\{delta=0 s='(.*)'\}")
fill = re.compile(r"PolyFillRectangle drawable=(0x[0-9a-f]+) gc=(0x[0-9a-f]+) "
                  r"rectangles=\{x=0 y=(\d+) w=\d+ h=13\}")
mapped = {}
title = {}
rows = {}
bars = {}
with open(trace, errors="replace") as f:
    for i, line in enumerate(f, 1):
        if i < start:
            continue
        m = text.search(line)
        if m:
            win, gc, x, y, s = m.group(1), m.group(2), int(m.group(3)), int(m.group(4)), m.group(5)
            s = s.replace("'},{", "")
            if y == 10:
                title[win] = s.strip()
            elif x == 0 and (y - 24) % 13 == 0:
                rows.setdefault(win, {})[(y - 24) // 13] = (s.rstrip(), gc)
            continue
        m = fill.search(line)
        if m:
            bars.setdefault(m.group(1), {})[(int(m.group(3)) - 13) // 13] = m.group(2)
        m = re.search(r"Request\(8\): MapWindow window=(0x[0-9a-f]+)", line)
        if m:
            mapped[m.group(1)] = True
        m = re.search(r"Request\(10\): UnmapWindow window=(0x[0-9a-f]+)", line)
        if m:
            mapped[m.group(1)] = False

for win, t in title.items():
    if want and not any(w in t for w in want):
        continue
    r = rows.get(win, {})
    last = max((k for k, (s, _) in r.items() if s), default=-1)
    print(f"== {win} [{t}] {'mapped' if mapped.get(win, True) else 'unmapped'}")
    for k in range(last + 1):
        s, gc = r.get(k, ("", ""))
        print(f"  {k:2d} {gc[-2:]} |{s}")
