#!/usr/bin/env python3
"""Minimal extractor for AIX backup-by-name (.bff) archives (uncompressed files only)."""
import os, re, struct, sys
d = open(sys.argv[1], 'rb').read()
out = sys.argv[2]
pat = re.compile(rb'(.)\x0b\x6b\xea', re.S)
n = 0
for m in pat.finditer(d):
    r = m.start(); L = d[r]
    if L < 8 or L > 64: continue
    mode = struct.unpack('<H', d[r+12:r+14])[0]
    size = struct.unpack('<I', d[r+24:r+28])[0]
    name = d[r+64:r+L*8].split(b'\0')[0]
    if not name.startswith(b'./'): continue
    ds = r + L*8
    # optional ACL/extension block precedes data
    if d[ds:ds+4] == b'\x02\x00\x00\x00' and d[ds+8:ds+12] == b'\x10\x00\x00\x00':
        ds += 40
    path = os.path.join(out, name.decode()[2:])
    if (mode & 0xf000) == 0x4000:
        os.makedirs(path, exist_ok=True); continue
    if (mode & 0xf000) != 0x8000: continue
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, 'wb').write(d[ds:ds+size]); n += 1
print(n, 'files')
