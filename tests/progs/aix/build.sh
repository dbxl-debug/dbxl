#!/bin/sh
# Build the AIX test programs, on the AIX 4.3.3 guest (docs/RECON.md).
#
#   test.s  hand-written PowerPC assembly for ../test.c
#   rich.s  the same for ../rich.c
#
# There is no C compiler on the guest; these mimic `xlc -g` output
# (XCOFF stabs), following the rules xldb needs (recon passes 2 and 4 in
# recon/xldb-observed.md).  Needs bos.adt.base (as) and bos.adt.syscalls
# (/usr/lib/syscalls.exp).
#
# Linked statically (-bnso): xldb 1.2.1 can't load AIX 4.3's shared
# libraries.  -bkeepfile keeps rich's unreferenced globals.
set -e
cd "$(dirname "$0")"

link="-bnso -bI:/usr/lib/syscalls.exp -bpT:0x10000000 -bpD:0x20000000"

as -o test.o test.s
ld -o test $link /usr/lib/crt0.o test.o -lc

as -o rich.o rich.s
ld -o rich -bkeepfile:rich.o $link /usr/lib/crt0.o rich.o -lc

echo "built: test rich"
