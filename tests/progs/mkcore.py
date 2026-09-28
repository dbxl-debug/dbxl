#!/usr/bin/env python3
"""mkcore.py PROGRAM FUNCTION CORE [SIGNO]

Write a core file of PROGRAM stopped at FUNCTION's first instruction, as
if it had died there of signal SIGNO (default 11, SIGSEGV): GDB's
generate-core-file records the breakpoint's SIGTRAP, so the signal in the
NT_PRSTATUS and NT_SIGINFO notes is patched afterwards.  Used for the
core-file tests (xldb -co, DESIGN.md 14); the host's own cores may go to
apport instead of a file.
"""
import struct
import subprocess
import sys

NT_PRSTATUS = 1
NT_SIGINFO = 0x53494749


def patch(path, signo):
    with open(path, "r+b") as f:
        data = bytearray(f.read())
        if data[:4] != b"\x7fELF" or data[4] != 2:
            sys.exit("mkcore: not a 64-bit ELF core")
        e_phoff, = struct.unpack_from("<Q", data, 0x20)
        e_phentsize, e_phnum = struct.unpack_from("<HH", data, 0x36)
        for i in range(e_phnum):
            ph = e_phoff + i * e_phentsize
            p_type, = struct.unpack_from("<I", data, ph)
            if p_type != 4:                       # PT_NOTE
                continue
            off, = struct.unpack_from("<Q", data, ph + 8)
            size, = struct.unpack_from("<Q", data, ph + 32)
            pos, end = off, off + size
            while pos + 12 <= end:
                namesz, descsz, ntype = struct.unpack_from("<III", data, pos)
                desc = pos + 12 + ((namesz + 3) & ~3)
                if ntype == NT_PRSTATUS:
                    # struct elf_prstatus: si_signo, si_code, si_errno,
                    # then short pr_cursig
                    struct.pack_into("<i", data, desc, signo)
                    struct.pack_into("<h", data, desc + 12, signo)
                elif ntype == NT_SIGINFO:
                    struct.pack_into("<iii", data, desc, signo, 0, 0)
                pos = desc + ((descsz + 3) & ~3)
        f.seek(0)
        f.write(data)


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    prog, func, core = sys.argv[1:4]
    signo = int(sys.argv[4]) if len(sys.argv) > 4 else 11
    r = subprocess.run(["gdb", "-q", "-batch", "-nx",
                        "-ex", "set startup-with-shell off",
                        "-ex", "break *" + func, "-ex", "run",
                        "-ex", "generate-core-file " + core, prog],
                       stdin=subprocess.DEVNULL, capture_output=True, text=True)
    if r.returncode != 0 or "Saved corefile" not in r.stdout + r.stderr:
        sys.exit("mkcore: gdb failed:\n" + r.stdout + r.stderr)
    patch(core, signo)


if __name__ == "__main__":
    main()
