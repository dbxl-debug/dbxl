#!/usr/bin/env python3
"""xdiff.py GOT REF [--crop X,Y,W,H] [--mask X,Y,W,H ...] [--out DIFF.png]

Compare two images pixel by pixel.  GOT is cropped with --crop (e.g. the
frame's region of a full-screen capture) before comparison; masked
rectangles (in REF coordinates) are ignored.  Prints the differing regions
and writes a diff image (differences in red over a dimmed copy of GOT,
zoomed 2x) when --out is given.  Exit status 0 when identical, 1 otherwise.
"""
import argparse
import sys

from PIL import Image, ImageChops


def rect(s):
    x, y, w, h = (int(v) for v in s.split(","))
    return x, y, w, h


def regions(mask_img):
    """Group differing pixels into bounding boxes (8px proximity)."""
    w, h = mask_img.size
    px = mask_img.load()
    boxes = []
    for y in range(h):
        for x in range(w):
            if not px[x, y]:
                continue
            for b in boxes:
                if b[0] - 8 <= x <= b[2] + 8 and b[1] - 8 <= y <= b[3] + 8:
                    b[0], b[1] = min(b[0], x), min(b[1], y)
                    b[2], b[3] = max(b[2], x), max(b[3], y)
                    b[4] += 1
                    break
            else:
                boxes.append([x, y, x, y, 1])
    return boxes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("got")
    ap.add_argument("ref")
    ap.add_argument("--crop", type=rect)
    ap.add_argument("--mask", type=rect, action="append", default=[])
    ap.add_argument("--out")
    a = ap.parse_args()

    got = Image.open(a.got).convert("RGB")
    ref = Image.open(a.ref).convert("RGB")
    if a.crop:
        x, y, w, h = a.crop
        got = got.crop((x, y, x + w, y + h))
    if got.size != ref.size:
        print(f"size mismatch: got {got.size}, ref {ref.size}")
        return 1

    diff = ImageChops.difference(got, ref).convert("L").point(lambda v: 255 if v else 0)
    for x, y, w, h in a.mask:
        diff.paste(0, (x, y, x + w, y + h))

    boxes = regions(diff)
    total = sum(b[4] for b in boxes)
    for b in boxes:
        print(f"  differs: x {b[0]}..{b[2]}  y {b[1]}..{b[3]}  ({b[4]} px)")
    print(f"{total} differing pixels ({len(a.mask)} masked regions)")

    if a.out:
        shown = Image.blend(got, Image.new("RGB", got.size), 0.6)
        shown.paste((255, 0, 0), (0, 0), diff)
        for x, y, w, h in a.mask:
            shown.paste((255, 255, 0), (x, y, x + w, y + 1))
            shown.paste((255, 255, 0), (x, y + h - 1, x + w, y + h))
        shown.resize((got.width * 2, got.height * 2), Image.NEAREST).save(a.out)
    return 0 if total == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
