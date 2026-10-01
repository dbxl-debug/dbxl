#!/usr/bin/env python3
"""putimages.py CONN_PREFIX

Decode every depth-1 PutImage request in a raw X11 client->server log
(CONN_PREFIX.c2s, written by rawproxy.py), using the image format from the
server's connection setup reply (CONN_PREFIX.s2c).  Prints each image as
rows of '#' (1) and '.' (0), keyed by drawable id and size.
"""
import struct
import sys

prefix = sys.argv[1]
c2s = open(prefix + ".c2s", "rb").read()
s2c = open(prefix + ".s2c", "rb").read()

# Client setup: byte-order byte then protocol version etc.
order = "<" if c2s[0:1] == b"l" else ">"
n_auth_name, n_auth_data = struct.unpack(order + "HH", c2s[6:10])
pad = lambda n: (n + 3) & ~3
pos = 12 + pad(n_auth_name) + pad(n_auth_data)

# Server setup reply: image byte order, bitmap bit order, scanline unit/pad.
(success,) = struct.unpack("B", s2c[0:1])
assert success == 1, "setup failed"
image_byte_order, bitmap_bit_order, scan_unit, scan_pad = struct.unpack(
    "BBBB", s2c[8 + 22:8 + 26])
lsb_bits = bitmap_bit_order == 0
lsb_bytes = image_byte_order == 0
print(f"# client order {order!r}; server image-byte-order={'LSB' if lsb_bytes else 'MSB'} "
      f"bit-order={'LSB' if lsb_bits else 'MSB'} unit={scan_unit} pad={scan_pad}")


def bit(data, stride, x, y):
    unit_bytes = scan_unit // 8
    ubase = y * stride + (x // scan_unit) * unit_bytes
    xin = x % scan_unit
    # byte within the unit that holds this pixel, per image byte order
    bidx = xin // 8 if lsb_bits else xin // 8
    if lsb_bits != lsb_bytes:
        bidx = unit_bytes - 1 - bidx
    b = data[ubase + bidx]
    return (b >> (xin % 8)) & 1 if lsb_bits else (b >> (7 - xin % 8)) & 1


while pos + 4 <= len(c2s):
    opcode = c2s[pos]
    minor = c2s[pos + 1]
    (length,) = struct.unpack(order + "H", c2s[pos + 2:pos + 4])
    if length == 0:        # BIG-REQUESTS
        (length,) = struct.unpack(order + "I", c2s[pos + 4:pos + 8])
    req = c2s[pos:pos + length * 4]
    if opcode == 72 and len(req) >= 24:
        drawable, gc, w, h, dx, dy, left_pad, depth = struct.unpack(
            order + "IIHHhhBB", req[4:22])
        data = req[24:]
        if depth == 1:
            bits_per_row = left_pad + w
            stride = ((bits_per_row + scan_pad - 1) // scan_pad) * scan_pad // 8
            print(f"\n== drawable=0x{drawable:08x} {w}x{h} format={minor} left_pad={left_pad}")
            for y in range(h):
                print("".join("#" if bit(data, stride, left_pad + x, y) else "."
                              for x in range(w)))
    pos += length * 4
