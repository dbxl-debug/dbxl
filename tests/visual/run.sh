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

# --- colour schemes and resources at startup -----------------------------
# References: xldb -bw, -wb, xldb.tagWindows, and the help's "messy" sample
# layout (-name messy-xldb), all debugging /usr/bin/bsh.  Resources come
# from a private $HOME/.Xdefaults.
config() {   # name xdefaults-file masks... -- dbxl-args...
    name=$1; xdef=$2; shift 2
    masks=
    while [ "$1" != "--" ]; do masks="$masks --mask $1"; shift; done
    shift
    home=$out/home-$name
    mkdir -p "$home"
    rm -f "$home/.Xdefaults"
    [ -n "$xdef" ] && cp "$ref/config/$xdef" "$home/.Xdefaults"
    kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true
    HOME=$home DISPLAY=$disp "$top/dbxl" "$@" >"$out/config-$name.log" 2>&1 &
    dbxl=$!
    i=0
    until DISPLAY=$disp xwininfo -name dbxl >/dev/null 2>&1; do
        i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl $name did not appear"; exit 1; }
        sleep 0.1
    done
    sleep 0.5
    DISPLAY=$disp python3 -c "
import time
from Xlib import display
d = display.Display()
d.screen().root.warp_pointer(0, 0); d.sync(); time.sleep(0.3)
d.screen().root.warp_pointer(640, 512); d.sync()"
    sleep 1
    python3 -c "from PIL import ImageGrab; ImageGrab.grab(xdisplay='$disp').save('$out/config-$name.png')"
    echo "config $name:"
    # shellcheck disable=SC2086
    if python3 "$top/tools/xdiff.py" "$out/config-$name.png" "$ref/config/$name.png" \
            $masks --out "$out/config-$name-diff.png"; then
        echo "  ok"
    else
        echo "  FAIL (see $out/config-$name-diff.png)"
        fail=1
    fi
}
# Default layout masks: the __start() line in Callers, the Source arrow.
config bw ""                      563,174,206,14 40,465,38,18 -- -bw
config wb ""                      563,174,206,14 40,465,38,18 -- -wb
config tags Xdefaults.tags        563,174,206,14 40,465,38,18 --
config messy Xdefaults.messy      119,238,236,13 41,565,38,18 -- -name messy-xldb
kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true; dbxl=

# --- milestone 2 UI scenario: menus, dialogs, strip, chips, messages ---
# Replays tests/visual/scenario.py (the script that recorded the xldb
# references) and compares every step.
kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true
DISPLAY=$disp python3 -c "
from Xlib import display
d = display.Display(); d.screen().root.warp_pointer(640, 512); d.sync()"
DISPLAY=$disp "$top/dbxl" >"$out/scenario.log" 2>&1 &
dbxl=$!
i=0
until DISPLAY=$disp xwininfo -name dbxl >/dev/null 2>&1; do
    i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl window did not appear"; exit 1; }
    sleep 0.1
done
sleep 1
rm -rf "$out/m2"
mkdir -p "$out/m2"
echo "milestone 2 scenario:"
if python3 "$top/tests/visual/scenario.py" "$disp" "$out/m2" --wait 0.4 >"$out/m2/scenario.log" 2>&1 &&
   python3 "$top/tests/visual/m2_compare.py" "$out/m2" "$ref/m2" "$out/m2"; then
    :
else
    echo "  FAIL (see $out/m2)"
    fail=1
fi
kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true; dbxl=

# --- milestone 3 scenario: debugging tests/progs/test under GDB ---------
# Run from the program's directory so its source is "./test.c", as in the
# xldb recording.  Compared with xldb where GDB stops at the same places
# and with reviewed dbxl captures elsewhere (see m3_compare.py).
m3() {   # display scale outdir
    d=$1; n=$2; o=$3
    DISPLAY=$d python3 -c "
from Xlib import display
d = display.Display(); d.screen().root.warp_pointer($((640 * n)), $((512 * n))); d.sync()"
    (cd "$top/tests/progs" && DISPLAY=$d exec "$top/dbxl" -scale "$n" ./test) \
        >"$o.log" 2>&1 &
    dbxl=$!
    i=0
    until DISPLAY=$d xwininfo -name "dbxl test" >/dev/null 2>&1; do
        i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl ./test did not appear"; exit 1; }
        sleep 0.1
    done
    sleep 2                              # GDB runs the program to main
    rm -rf "$o"
    mkdir -p "$o"
    if python3 "$top/tests/visual/scenario_m3.py" "$d" "$o" --wait 1 --scale "$n" \
            >"$o/scenario.log" 2>&1 &&
       python3 "$top/tests/visual/m3_compare.py" "$o" "$ref" "$o" "$n"; then
        :
    else
        echo "  FAIL (see $o)"
        fail=1
    fi
    kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true; dbxl=
}
echo "milestone 3 scenario:"
m3 "$disp" 1 "$out/m3"

# --- milestone 4 scenario: data panes and variable menus, debugging rich --
m4() {   # display scale outdir
    d=$1; n=$2; o=$3
    DISPLAY=$d python3 -c "
from Xlib import display
d = display.Display(); d.screen().root.warp_pointer($((640 * n)), $((512 * n))); d.sync()"
    (cd "$top/tests/progs" && DISPLAY=$d exec "$top/dbxl" -scale "$n" ./rich) \
        >"$o.log" 2>&1 &
    dbxl=$!
    i=0
    until DISPLAY=$d xwininfo -name "dbxl rich" >/dev/null 2>&1; do
        i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl ./rich did not appear"; exit 1; }
        sleep 0.1
    done
    sleep 2
    rm -rf "$o"
    mkdir -p "$o"
    if python3 "$top/tests/visual/scenario_m4.py" "$d" "$o" --wait 1 --scale "$n" \
            >"$o/scenario.log" 2>&1 &&
       python3 "$top/tests/visual/m4_compare.py" "$o" "$ref" "$o" "$n"; then
        :
    else
        echo "  FAIL (see $o)"
        fail=1
    fi
    kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true; dbxl=
}
echo "milestone 4 scenario:"
m4 "$disp" 1 "$out/m4"

# --- milestone 5a scenario: breakpoints and machine-level panes ----------
m5() {   # display scale outdir
    d=$1; n=$2; o=$3
    DISPLAY=$d python3 -c "
from Xlib import display
d = display.Display(); d.screen().root.warp_pointer($((640 * n)), $((512 * n))); d.sync()"
    (cd "$top/tests/progs" && DISPLAY=$d exec "$top/dbxl" -scale "$n" ./rich) \
        >"$o.log" 2>&1 &
    dbxl=$!
    i=0
    until DISPLAY=$d xwininfo -name "dbxl rich" >/dev/null 2>&1; do
        i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl ./rich did not appear"; exit 1; }
        sleep 0.1
    done
    sleep 2
    rm -rf "$o"
    mkdir -p "$o"
    if python3 "$top/tests/visual/scenario_m5.py" "$d" "$o" --wait 1.2 --scale "$n" \
            >"$o/scenario.log" 2>&1 &&
       python3 "$top/tests/visual/m5_compare.py" "$o" "$ref" "$o" "$n"; then
        :
    else
        echo "  FAIL (see $o)"
        fail=1
    fi
    kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true; dbxl=
}
echo "milestone 5a scenario:"
m5 "$disp" 1 "$out/m5"

# --- milestone 5b scenario: options files, variable menu, Help -----------
# It saves tests/progs/.dbxl.rich (loaded at the first stop if present, so
# removed first) and /tmp/layout.
m6() {   # display scale outdir
    d=$1; n=$2; o=$3
    rm -f "$top/tests/progs/.dbxl.rich" /tmp/layout
    DISPLAY=$d python3 -c "
from Xlib import display
d = display.Display(); d.screen().root.warp_pointer($((640 * n)), $((512 * n))); d.sync()"
    (cd "$top/tests/progs" && DISPLAY=$d exec "$top/dbxl" -scale "$n" ./rich) \
        >"$o.log" 2>&1 &
    dbxl=$!
    i=0
    until DISPLAY=$d xwininfo -name "dbxl rich" >/dev/null 2>&1; do
        i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl ./rich did not appear"; exit 1; }
        sleep 0.1
    done
    sleep 2
    rm -rf "$o"
    mkdir -p "$o"
    if python3 "$top/tests/visual/scenario_m6.py" "$d" "$o" --wait 1.2 --scale "$n" \
            >"$o/scenario.log" 2>&1 &&
       python3 "$top/tests/visual/m6_compare.py" "$o" "$ref" "$o" "$n"; then
        :
    else
        echo "  FAIL (see $o)"
        fail=1
    fi
    kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true; dbxl=
}
echo "milestone 5b scenario:"
m6 "$disp" 1 "$out/m6"

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
    # Settle, then move the pointer out and into Source so it gets an Enter.
    sleep 0.5
    DISPLAY=$big python3 -c "
import time
from Xlib import display
d = display.Display()
d.screen().root.warp_pointer(0, 0)
d.sync()
time.sleep(0.3)
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

# --- milestone 2 scenario at scale 2 ------------------------------------
DISPLAY=$big python3 -c "
from Xlib import display
d = display.Display(); d.screen().root.warp_pointer(1281, 1025); d.sync()"
DISPLAY=$big "$top/dbxl" -scale 2 >"$out/scenario-x2.log" 2>&1 &
dbxl=$!
i=0
until DISPLAY=$big xwininfo -name dbxl >/dev/null 2>&1; do
    i=$((i + 1)); [ $i -gt 50 ] && { echo "dbxl -scale 2 did not appear"; exit 1; }
    sleep 0.1
done
sleep 1
rm -rf "$out/m2-x2"
mkdir -p "$out/m2-x2"
echo "milestone 2 scenario -scale 2:"
if python3 "$top/tests/visual/scenario.py" "$big" "$out/m2-x2" --wait 0.4 --scale 2 \
        >"$out/m2-x2/scenario.log" 2>&1 &&
   python3 "$top/tests/visual/m2_compare.py" "$out/m2-x2" "$ref/m2" "$out/m2-x2" 2 \
        | tail -1; then
    :
else
    echo "  FAIL (see $out/m2-x2)"
    fail=1
fi
kill "$dbxl" 2>/dev/null || true; wait "$dbxl" 2>/dev/null || true; dbxl=

echo "milestone 3 scenario -scale 2:"
m3 "$big" 2 "$out/m3-x2"

echo "milestone 4 scenario -scale 2:"
m4 "$big" 2 "$out/m4-x2"

echo "milestone 5a scenario -scale 2:"
m5 "$big" 2 "$out/m5-x2"

echo "milestone 5b scenario -scale 2:"
m6 "$big" 2 "$out/m6-x2"

exit $fail
