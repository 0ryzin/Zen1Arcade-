#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds Zen1's own tiny test cartridges, hand-assembled here, for systems
# where no freely redistributable test ROM was at hand:
#   zen1-test-gg.gg    Sega Game Gear: GG colour RAM (12-bit), mode 4, and a
#                      PSG tone on the left speaker only (GG stereo)
#                      tiles, a checkerboard in four colours
#   zen1-test-gbc.gbc  Game Boy Color: CGB palette RAM (15-bit), tiles in
#                      four colours, diagonal stripes
#   zen1-test-sg1000.sg SG-1000: TMS9918 backdrop colour (blue, red while
#                      button 1 is held) and a PSG tone
#   zen1-test-2600.a26 Atari 2600: colour bars (the TIA's whole palette),
#                      shifted while Fire is held, and a steady tone
# Each picture is known exactly, so a test can check the colours that come
# out. They contain no third-party code or data (no logos, no BIOS).
#
# usage: make_test_roms.py OUTPUT_DIR
import os
import sys


def game_gear():
    rom = bytearray(0x8000)
    rom[0:3] = bytes([0xC3, 0x00, 0x01])          # jp $0100
    rom[0x66:0x68] = bytes([0xED, 0x45])          # retn (pause NMI)
    regs = bytes([0x04, 0x80, 0x80, 0x81, 0xFF, 0x82, 0xFF, 0x83, 0xFF, 0x84, 0xFF, 0x85,
                  0xFB, 0x86, 0x00, 0x87, 0x00, 0x88, 0x00, 0x89, 0xFF, 0x8A])
    # Colours: GGGGRRRR, ----BBBB. 0 dark blue, 1 white, 2 red, 3 green.
    pal = bytes([0x00, 0x08, 0xFF, 0x0F, 0x0F, 0x00, 0xF0, 0x00])
    tiles = bytes([0xFF, 0x00, 0x00, 0x00] * 8 +   # tile 1: colour 1
                  [0x00, 0xFF, 0x00, 0x00] * 8 +   # tile 2: colour 2
                  [0xFF, 0xFF, 0x00, 0x00] * 8)    # tile 3: colour 3
    row_a = b"".join(bytes([1 + (x % 3), 0]) for x in range(32))
    row_b = b"".join(bytes([0 if x % 2 else 1, 0]) for x in range(32))
    data = 0x0200
    at = {}
    blob = bytearray()
    for name, b in (("regs", regs), ("pal", pal), ("tiles", tiles), ("row_a", row_a), ("row_b", row_b)):
        at[name] = data + len(blob)
        blob += b
    rom[data:data + len(blob)] = blob

    def w(addr):
        return bytes([addr & 0xFF, addr >> 8])

    out = lambda v, port=0xBF: bytes([0x3E, v, 0xD3, port])  # ld a,v; out (port),a
    otir = lambda src, n, port: bytes([0x21]) + w(src) + bytes([0x06, n, 0x0E, port, 0xED, 0xB3])
    code = bytearray()
    code += bytes([0xF3, 0xED, 0x56, 0x31]) + w(0xDFF0)            # di; im 1; ld sp,$dff0
    code += otir(at["regs"], len(regs), 0xBF)                        # VDP registers
    code += out(0x00) + out(0xC0)                                    # CRAM address 0
    code += otir(at["pal"], len(pal), 0xBE)
    code += out(0x20) + out(0x40)                                    # VRAM $0020 (tile 1)
    code += otir(at["tiles"], len(tiles), 0xBE)
    code += out(0x00) + out(0x78)                                    # VRAM $3800 (names)
    code += bytes([0x16, 14])                                        # ld d,14
    loop = len(code)
    code += otir(at["row_a"], 64, 0xBE) + otir(at["row_b"], 64, 0xBE)
    code += bytes([0x15])                                            # dec d
    code += bytes([0x20, (loop - (len(code) + 2)) & 0xFF])           # jr nz,loop
    # Sound: a steady tone on PSG channel 0, sent to the LEFT speaker only
    # (Game Gear stereo register, port $06: bits 4-7 left, 0-3 right).
    code += out(0x10, 0x06)
    code += out(0x8E, 0x7F) + out(0x0F, 0x7F) + out(0x90, 0x7F)     # tone $0FE, volume 0 (loudest)
    code += out(0xC0) + out(0x81)                                    # display on
    code += bytes([0x18, 0xFE])                                      # jr $
    rom[0x100:0x100 + len(code)] = code
    rom[0x7FF0:0x7FF8] = b"TMR SEGA"
    s = sum(rom[0:0x7FF0]) & 0xFFFF
    rom[0x7FFA:0x7FFC] = bytes([s & 0xFF, s >> 8])
    rom[0x7FFF] = 0x7C  # Game Gear export, 32 KB
    return bytes(rom)


def game_boy_color():
    rom = bytearray(0x8000)
    rom[0x100:0x104] = bytes([0x00, 0xC3, 0x50, 0x01])  # nop; jp $0150
    rom[0x134:0x13D] = b"ZEN1 TEST"
    rom[0x143] = 0x80   # works on CGB (colour)
    rom[0x147] = 0x00   # ROM only
    rom[0x148] = 0x00   # 32 KB
    x = 0
    for i in range(0x134, 0x14D):
        x = (x - rom[i] - 1) & 0xFF
    rom[0x14D] = x
    # Colours, RGB555 little-endian: 0 blue, 1 white, 2 red, 3 green.
    pal = bytes([0x00, 0x7C, 0xFF, 0x7F, 0x1F, 0x00, 0xE0, 0x03])
    tiles = bytes([0xFF, 0x00] * 8 + [0x00, 0xFF] * 8 + [0xFF, 0xFF] * 8)
    rows = bytes(((x + r) & 3) for r in range(4) for x in range(32))
    data = 0x0400
    at = {"pal": data, "tiles": data + 16, "rows": data + 64}
    rom[at["pal"]:at["pal"] + 8] = pal
    rom[at["tiles"]:at["tiles"] + len(tiles)] = tiles
    rom[at["rows"]:at["rows"] + len(rows)] = rows

    def w(addr):
        return bytes([addr & 0xFF, addr >> 8])

    code = bytearray()
    code += bytes([0xF3, 0x31]) + w(0xDFF0)                          # di; ld sp,$dff0
    code += bytes([0xF0, 0x44, 0xFE, 144, 0x38, 0xFA])               # wait for line 144
    code += bytes([0xAF, 0xE0, 0x40])                                # LCD off
    code += bytes([0x3E, 0x80, 0xE0, 0x68])                          # BCPS: index 0, auto-increment
    code += bytes([0x21]) + w(at["pal"]) + bytes([0x06, 8])          # ld hl,pal; ld b,8
    code += bytes([0x2A, 0xE0, 0x69, 0x05, 0x20, 0xFA])              # ld a,(hl+); ldh ($69),a; dec b; jr nz
    code += bytes([0x21]) + w(0x8010) + bytes([0x11]) + w(at["tiles"]) + bytes([0x06, len(tiles)])
    code += bytes([0x1A, 0x22, 0x13, 0x05, 0x20, 0xFA])              # copy tiles
    code += bytes([0x21]) + w(0x9800) + bytes([0x3E, 8, 0xEA]) + w(0xC000)
    outer = len(code)
    code += bytes([0x11]) + w(at["rows"]) + bytes([0x06, 128])
    code += bytes([0x1A, 0x22, 0x13, 0x05, 0x20, 0xFA])              # copy 4 map rows
    code += bytes([0xFA]) + w(0xC000) + bytes([0x3D, 0xEA]) + w(0xC000)
    code += bytes([0x20, (outer - (len(code) + 2)) & 0xFF])          # jr nz,outer
    code += bytes([0x3E, 0x91, 0xE0, 0x40])                          # LCD on, BG on
    code += bytes([0x18, 0xFE])                                      # jr $
    rom[0x150:0x150 + len(code)] = code
    return bytes(rom)


def sg_1000():
    """8 KB SG-1000 cartridge (TMS9918 VDP): the backdrop is dark blue, red
    while button 1 is held, and a steady PSG tone plays."""
    rom = bytearray([0xFF] * 0x2000)
    out = lambda v, port=0xBF: bytes([0x3E, v, 0xD3, port])  # ld a,v; out (port),a
    c = bytearray()
    c += bytes([0xF3, 0x31, 0xF0, 0xC3])                         # di; ld sp,$c3f0
    c += out(0x00) + out(0x80)                                   # reg 0: graphics I
    c += out(0xC0) + out(0x81)                                   # reg 1: 16K, display on, no IRQ
    c += out(0x8E, 0x7F) + out(0x0F, 0x7F) + out(0x90, 0x7F)    # PSG: tone $0FE, loudest
    loop = len(c)
    c += bytes([0xDB, 0xDC, 0xE6, 0x10])                         # in a,($dc); and $10 (button 1, low = held)
    c += bytes([0x3E, 0x04, 0x28, 0x02, 0x18, 0x02, 0x3E, 0x08])  # a = 4 (blue); jr z -> a = 8 (red)
    c += bytes([0xD3, 0xBF, 0x3E, 0x87, 0xD3, 0xBF])             # out reg 7 (backdrop) = a
    c += bytes([0x18, (loop - (len(c) + 2)) & 0xFF])             # jr loop
    rom[0:len(c)] = c
    return bytes(rom)


def atari_2600():
    """4 KB cartridge: colour bars that change while Fire is held, and a
    steady tone on channel 0."""
    rom = bytearray([0xEA] * 0x1000)
    VSYNC, VBLANK, WSYNC, COLUBK, INPT4 = 0x00, 0x01, 0x02, 0x09, 0x0C
    AUDC0, AUDF0, AUDV0 = 0x15, 0x17, 0x19
    c = bytearray()
    c += bytes([0x78, 0xD8, 0xA2, 0xFF, 0x9A])                # sei; cld; ldx #$ff; txs
    c += bytes([0xA9, 0x00, 0x95, 0x00, 0xCA, 0xD0, 0xFB])     # clear $00-$FF: sta 0,x; dex; bne
    c += bytes([0xA9, 0x04, 0x85, AUDC0, 0xA9, 0x0C, 0x85, AUDF0, 0xA9, 0x08, 0x85, AUDV0])
    frame = len(c)
    c += bytes([0xA9, 0x02, 0x85, VBLANK, 0x85, VSYNC])         # VBLANK and VSYNC on
    c += bytes([0x85, WSYNC, 0x85, WSYNC, 0x85, WSYNC])
    c += bytes([0xA9, 0x00, 0x85, VSYNC])                       # VSYNC off
    c += bytes([0xA2, 37, 0x85, WSYNC, 0xCA, 0xD0, 0xFB])      # 37 lines of blanking
    c += bytes([0xA5, INPT4, 0x29, 0x80, 0x49, 0x80, 0x85, 0x80])  # $80 = $80 while Fire is held
    c += bytes([0xA9, 0x00, 0x85, VBLANK])                      # picture on
    c += bytes([0xA2, 192])                                     # 192 lines:
    line = len(c)
    c += bytes([0x8A, 0x45, 0x80, 0x85, COLUBK, 0x85, WSYNC, 0xCA])  # txa; eor $80; sta COLUBK; sta WSYNC; dex
    c += bytes([0xD0, (line - (len(c) + 2)) & 0xFF])            # bne line
    c += bytes([0xA9, 0x02, 0x85, VBLANK, 0xA2, 30, 0x85, WSYNC, 0xCA, 0xD0, 0xFB])  # 30 lines overscan
    c += bytes([0x4C, frame & 0xFF, 0xF0 + (frame >> 8)])       # jmp frame
    rom[0:len(c)] = c
    rom[0xFFC:0x1000] = bytes([0x00, 0xF0, 0x00, 0xF0])        # reset, irq -> $F000
    return bytes(rom)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    for name, data in (("zen1-test-gg.gg", game_gear()), ("zen1-test-gbc.gbc", game_boy_color()),
                       ("zen1-test-2600.a26", atari_2600()), ("zen1-test-sg1000.sg", sg_1000())):
        with open(os.path.join(out, name), "wb") as f:
            f.write(data)
        print(os.path.join(out, name), len(data))


if __name__ == "__main__":
    main()
