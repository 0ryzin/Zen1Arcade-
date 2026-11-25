#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Writes a tiny Mega Drive test cartridge (no game data) as OUT.md and OUT.smd.
# Its header declares battery SRAM; its code stores 0x5A and 0xA5 in the first
# two SRAM bytes and stops. Used to check .smd de-interleaving and SRAM saves.
# usage: make_md_sram_test.py OUT_STEM
import struct, sys

rom = bytearray(0x20000)
struct.pack_into(">II", rom, 0, 0x00FFFE00, 0x200)
rom[0x100:0x110] = b"SEGA MEGA DRIVE "
rom[0x150:0x160] = b"ZEN1 SRAM TEST  "
rom[0x1B0:0x1B2] = b"RA"
struct.pack_into(">HII", rom, 0x1B2, 0xF820, 0x200001, 0x20FFFF)
rom[0x1F0:0x1F3] = b"JUE"
code = bytes.fromhex("13FC005A00200001" "13FC00A500200003" "60FE")  # move.b x2, bra.s *
rom[0x200:0x200 + len(code)] = code
open(sys.argv[1] + ".md", "wb").write(rom)
smd = bytearray(0x200)
smd[0], smd[8], smd[9] = len(rom) // 0x4000, 0xAA, 0xBB
for b in range(0, len(rom), 0x4000):
    blk = rom[b:b + 0x4000]
    smd += blk[1::2] + blk[0::2]
open(sys.argv[1] + "-smd.smd", "wb").write(smd)
