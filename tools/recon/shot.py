#!/usr/bin/env python3
"""shot.py NAME [x y [button]] ...  -- optional pointer action, then capture :42.

Actions (applied in order before capture, coordinates are root-relative):
  move X Y      move pointer
  click X Y B   click button B at X,Y
  key KEYS      xdotool key KEYS
  wait SECS
Saves NAME.png (full screen) and NAME-map.txt (mapped windows).
"""
import subprocess, sys, time, os
from PIL import ImageGrab

env = dict(os.environ, DISPLAY=":42")


def xdo(*a):
    subprocess.run(["xdotool", *map(str, a)], env=env, check=True)


def mapped_tree():
    out = subprocess.run(["xwininfo", "-root", "-tree"], env=env,
                         capture_output=True, text=True).stdout
    keep = []
    for line in out.splitlines():
        s = line.strip()
        if s.startswith("0x"):
            wid = s.split()[0]
            st = subprocess.run(["xwininfo", "-id", wid], env=env,
                                capture_output=True, text=True).stdout
            if "IsViewable" in st:
                keep.append(line)
    return "\n".join(keep)


name = sys.argv[1]
args = sys.argv[2:]
i = 0
while i < len(args):
    a = args[i]
    if a == "move":
        xdo("mousemove", args[i+1], args[i+2]); i += 3
    elif a == "click":
        xdo("mousemove", args[i+1], args[i+2]); time.sleep(0.3)
        xdo("click", args[i+3]); i += 4
    elif a == "down":
        xdo("mousemove", args[i+1], args[i+2]); time.sleep(0.3)
        xdo("mousedown", args[i+3]); i += 4
    elif a == "up":
        xdo("mousemove", args[i+1], args[i+2]); time.sleep(0.3)
        xdo("mouseup", args[i+3]); i += 4
    elif a == "key":
        xdo("key", args[i+1]); i += 2
    elif a == "wait":
        time.sleep(float(args[i+1])); i += 2
    else:
        sys.exit(f"bad action {a}")
time.sleep(1.5)
ImageGrab.grab(xdisplay=":42").save(name + ".png")
open(name + "-map.txt", "w").write(mapped_tree() + "\n")
print("saved", name)
