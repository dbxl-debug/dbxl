# dbxl debugger

`dbxl` is a Linux clone of IBM's AIX `xldb` graphical debugger, written in C
with plain Xlib, with GDB/MI as the debugger backend.

See [docs/DESIGN.md](docs/DESIGN.md) for the design and the milestones.

## Building

Needs a C11 compiler, the libX11 and libXrender development files and
`pkg-config`.

    make

## Running

    ./dbxl [-display Display] [-name Name] [-font Font] [-geometry Geometry]
           [-title Title] [-bg Color] [-fg Color] [-bw] [-wb]
           [-scale auto|1|2|3|4] [Program [Args...]]

The user interface is in place (windows, chips, Window Control and Options
menus, dialogs, move/resize, colour schemes, `.Xdefaults` resources such as
`dbxl.commandList`, `dbxl.tagWindows` and per-window geometries); debugging a
program arrives with the GDB backend in milestone 3.

`-scale N` enlarges everything by an exact integer factor for high resolution
screens (each xldb pixel becomes an NxN block). `-scale auto` picks the
factor from the screen's DPI (`Xft.dpi` if set) and shrinks it until the
frame fits on the screen. The default is 1, which matches xldb exactly.
Geometries (`-geometry`, and later `.Xdefaults`) stay in xldb's 1x units.

## Tests

    make visual

This runs `dbxl` on private `Xvfb` servers and compares screenshots against
captures of the real xldb in `tests/visual/ref/`, at scale 1 and, enlarged,
at scales 2-4. It needs `Xvfb`, `xwininfo` and Python 3 with Pillow and
python-xlib. The full run takes a few minutes: most of it replays a 33-step
scenario that was recorded against the real xldb, at scales 1 and 2.
