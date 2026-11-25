#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds a test zip for an arcade set whose files have exactly the names,
# sizes and CRC-32s the engine expects, but contain no game: zeros (or
# random bytes with --random SEED) and four bytes that steer the CRC. It
# exercises the whole launch path (identification, loading, machine start,
# frontend hand-over) without any copyrighted ROM data.
#
# usage: make_crc_test_set.py ENGINE SET OUT.zip [--random SEED]
import random
import subprocess
import sys
import zipfile
import zlib

TABLE = []
for n in range(256):
    c = n
    for _ in range(8):
        c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
    TABLE.append(c)
INV = {TABLE[k] >> 24: k for k in range(256)}


def forge(size, target, rnd):
    d = bytearray(rnd.randbytes(size) if rnd else bytes(size))
    state = (~zlib.crc32(bytes(d[:-4]))) & 0xFFFFFFFF
    reg = (~target) & 0xFFFFFFFF
    for _ in range(4):
        k = INV[reg >> 24]
        reg = ((reg ^ TABLE[k]) << 8 | k) & 0xFFFFFFFF
    x = reg ^ state
    d[-4:] = bytes((x >> (8 * j)) & 0xFF for j in range(4))
    assert zlib.crc32(bytes(d)) == target
    return bytes(d)


def main():
    engine, set_, out = sys.argv[1:4]
    rnd = random.Random(int(sys.argv[5])) if len(sys.argv) > 5 and sys.argv[4] == "--random" else None
    listing = subprocess.run([engine, "--rom-files", set_], check=True, capture_output=True, text=True).stdout
    with zipfile.ZipFile(out, "w") as z:
        for line in listing.splitlines():
            name, crc, size = line.split()
            z.writestr(name, forge(int(size), int(crc, 16), rnd))
    print(out)


main()
