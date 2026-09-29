# dbxl debugger

`dbxl` is a Linux clone of IBM's AIX `xldb` graphical debugger, written in C
with plain Xlib, with GDB/MI as the debugger backend.

See [docs/DESIGN.md](docs/DESIGN.md) for the design and the milestones.

![dbxl stopped at a breakpoint in tests/progs/rich, with a pointer
expanded in the Locals window](docs/screenshot.png)

## Prereqs

On Debian based systems:

```
sudo apt update && sudo apt install build-essential pkg-config \
  libx11-dev libxrender-dev gdb xfonts-base
```

If you also want to run the tests:

```
sudo apt install xvfb x11-utils python3 python3-pil python3-xlib
```

## Building

Needs a C11 compiler, the libX11 and libXrender development files and
`pkg-config`.

    make

## Running

    ./dbxl [-display Display] [-name Name] [-font Font] [-geometry Geometry]
           [-title Title] [-bg Color] [-fg Color] [-bw] [-wb]
           [-scale auto|1|2|3|4] [-E Command] [-F Command] [-I Directory]
           [-a ProcessId] [-c MaxCalls] [-co] [-e MaxArrayElements]
           [-i Signal] [-k] [-n] [-q] [-r SubprogramName] [-v] [-h]
           [Program [ProgramArgument...]]

dbxl runs the program under GDB and stops it in `main` (or the subprogram
given with `-r`). `-a` attaches to a running process instead, and `-co`
shows the core file `./core` of the program. `./dbxl -h` prints the full
help, which is also in the Help window.

All of xldb's windows are there, with their menus and dialogs:
- Source, Callers and Locals;
- Globals and Monitor;
- Breakpoints, including conditional ones;
- Disassembly, Registers, Storage and Threads;
- Files and Subprograms;
- Help and Messages.

Resources in `.Xdefaults` work as in xldb, as `dbxl.<resource>`; for
example `dbxl.commandList`, the layout saved by "Save layout" in the
Options menu, and the value styles.

`-scale N` enlarges everything by an exact integer factor for high resolution
screens (each xldb pixel becomes an NxN block). `-scale auto` picks the
factor from the screen's DPI (`Xft.dpi` if set) and shrinks it until the
frame fits on the screen. The default is 1, which matches xldb exactly.
Geometries (`-geometry` and the layout resources) stay in xldb's 1x units.

## Tests

    make test
    make visual

`make test` runs the unit tests: the GDB/MI parser, value formatting and
input checking.

`make visual` builds the test programs in `tests/progs/` and runs `dbxl` on
private `Xvfb` servers under GDB. It compares screenshots with captures of
the real xldb in `tests/visual/ref/`:
- the start-up display and colour schemes, at scales 1 to 4;
- scenarios recorded against the real xldb, one or two per milestone
  (`tests/visual/scenario*.py`), at scales 1 and 2.

Where AIX and Linux can't match (machine code, addresses, uninitialised
values), those regions are masked. The full run takes about 15 minutes.

The same tests run in GitHub Actions on every pull request, in an Ubuntu
26.04 container (`.github/workflows/ci.yml`), and must pass before merging
into `main`.

## Acknowledgements

This project builds on, and simply would not have been possible without
Artyom Tarasenko's work enabling IBM AIX to run under QEMU's PowerPC/PReP
(IBM 40p) emulation. In particular, it uses his patched QEMU branch
[`40p-20260308-aix-boots`](https://github.com/artyom-tarasenko/qemu/tree/40p-20260308-aix-boots)
and
[custom Open Firmware implementation](https://github.com/artyom-tarasenko/openfirmware/releases/tag/40p-20190413).
His
[AIX/PReP under QEMU How-To](https://tyom.blogspot.com/2019/04/aixprep-under-qemu-how-to.html)
provided the foundation for building QEMU that would work with AIX 4.3.3 and
which was used to replicate the amazing visual style of `xldb`. Many thanks to
Artyom for developing and documenting this work.
