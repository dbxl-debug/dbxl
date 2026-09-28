# dbxl debugger

`dbxl` is a Linux clone of IBM's AIX `xldb` graphical debugger, written in C
with plain Xlib, with GDB/MI as the debugger backend.

See [docs/DESIGN.md](docs/DESIGN.md) for the design and the milestones.

## Building

Needs a C11 compiler, the libX11 development files and `pkg-config`.

    make

## Running

    ./dbxl [-display Display] [-font Font] [-geometry Geometry] [-title Title] [Program [Args...]]

Milestone 1 shows the default window layout only; no program is debugged yet.

## Tests

    make visual

This runs `dbxl` on a private `Xvfb` and compares screenshots against
captures of the real xldb in `tests/visual/ref/`. It needs `Xvfb`,
`xwininfo` and Python 3 with Pillow.
