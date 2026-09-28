#!/bin/sh
# Visual regression tests: run dbxl on a private Xvfb and compare captures
# against reference images of the real xldb (see DESIGN.md 13).
#
# Needs: Xvfb, xwininfo, python3 with Pillow and python-xlib.
set -eu

top=$(cd "$(dirname "$0")/../.." && pwd)
ref=$top/tests/visual/ref
out=${VISUAL_OUT:-$top/build/visual}
disp=${VISUAL_DISPLAY:-:98}
mkdir -p "$out"

Xvfb "$disp" -screen 0 1280x1024x24 -nolisten tcp >"$out/xvfb.log" 2>&1 &
xvfb=$!
dbxl=
cleanup() {
    [ -n "$dbxl" ] && kill "$dbxl" 2>/dev/null || true
    kill "$xvfb" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Wait for the server.
i=0
until DISPLAY=$disp xwininfo -root >/dev/null 2>&1; do
    i=$((i + 1)); [ $i -gt 50 ] && { echo "Xvfb did not start"; exit 1; }
    sleep 0.1
done

fail=0

# --- startup: default layout with no program -----------------------------
# The pointer starts at the screen centre (640,512), inside Source, as in
# the reference capture, so Source has the active (white) border.
DISPLAY=$disp "$top/dbxl" >"$out/startup.log" 2>&1 &
dbxl=$!
i=0
until DISPLAY=$disp xwininfo -name dbxl >/dev/null 2>&1; do
    i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl window did not appear"; exit 1; }
    sleep 0.1
done
sleep 1
python3 -c "from PIL import ImageGrab; ImageGrab.grab(xdisplay='$disp').save('$out/startup.png')"

# Masks (reference coordinates) for content that needs a running program:
#   the selected __start() line in Callers, the execution arrow in Source.
echo "startup:"
if python3 "$top/tools/xdiff.py" "$out/startup.png" "$ref/xldb-startup.png" \
        --crop 33,73,959,848 \
        --mask 530,101,206,14 \
        --mask 7,392,38,18 \
        --out "$out/startup-diff.png"; then
    echo "  ok"
else
    echo "  FAIL (see $out/startup-diff.png)"
    fail=1
fi

# --- startup at scales 2-4: must equal the reference enlarged NxN ---------
# The frame's logical geometry is multiplied by N, so it is at (33N, 73N).
# The pointer is placed at logical (640,512), inside Source, as at scale 1.
big=${VISUAL_BIG_DISPLAY:-:97}
Xvfb "$big" -screen 0 4096x3840x24 -nolisten tcp >"$out/xvfb-big.log" 2>&1 &
xvfb_big=$!
trap 'cleanup; kill $xvfb_big 2>/dev/null || true' EXIT INT TERM
i=0
until DISPLAY=$big xwininfo -root >/dev/null 2>&1; do
    i=$((i + 1)); [ $i -gt 50 ] && { echo "big Xvfb did not start"; exit 1; }
    sleep 0.1
done
for n in 2 3 4; do
    DISPLAY=$big "$top/dbxl" -scale $n >"$out/startup-x$n.log" 2>&1 &
    dbxl=$!
    i=0
    until DISPLAY=$big xwininfo -name dbxl >/dev/null 2>&1; do
        i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl -scale $n did not appear"; exit 1; }
        sleep 0.1
    done
    DISPLAY=$big python3 -c "
from Xlib import display
d = display.Display()
d.screen().root.warp_pointer($((640 * n)), $((512 * n)))
d.sync()"
    sleep 1
    python3 -c "from PIL import ImageGrab; ImageGrab.grab(xdisplay='$big').save('$out/startup-x$n.png')"
    kill "$dbxl"; wait "$dbxl" 2>/dev/null || true; dbxl=
    echo "startup -scale $n:"
    if python3 "$top/tools/xdiff.py" "$out/startup-x$n.png" "$ref/xldb-startup.png" \
            --crop $((33 * n)),$((73 * n)),$((959 * n)),$((848 * n)) \
            --ref-scale $n \
            --mask 530,101,206,14 \
            --mask 7,392,38,18 \
            --out "$out/startup-x$n-diff.png"; then
        echo "  ok"
    else
        echo "  FAIL (see $out/startup-x$n-diff.png)"
        fail=1
    fi
done

exit $fail
