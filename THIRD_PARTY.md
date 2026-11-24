# Third-party components

Everything below is compatible with Zen1 Arcade's GPL-3.0-or-later licence.
Each vendored component keeps its own licence file next to its code.

| Component | Purpose | Licence | How it is used | Where |
| --- | --- | --- | --- | --- |
| Moira (Dirk W. Hoffmann) | Motorola 68000 CPU | MIT | Vendored, static | `third_party/moira` (+ `VERSION`, `LICENSE`) |
| z80 (superzazu) | Zilog Z80 CPU | MIT | Vendored, static | `third_party/z80` |
| ymfm (Aaron Giles) | Yamaha YM2151 FM sound | BSD-3-Clause | Vendored, static | `third_party/ymfm` |
| MAME CPS1 driver (Paul Leaman and MAME contributors) | CPS1 memory map, board data, video rules: **adapted** into Zen1 code | BSD-3-Clause | Rewritten/adapted source in `machines/cps1` | `machines/cps1/LICENSE-MAME-BSD-3-Clause.txt` |
| SDL3 | Engine window, rendering, audio, game input; intro sound in the frontend | zlib | Dynamic (bundled in the AppImage) | system / AppImage |
| Qt 6 (Core, Gui, Qml, Quick) | Frontend | LGPL-3.0 | Dynamic (bundled in the AppImage) | system / AppImage |
| zlib | Reading .zip ROM sets | zlib | Dynamic | system |

Written by Zen1 itself (no third-party code): the OKI MSM6295 ADPCM sound
chip, the CPS1 renderer and machine glue, the ROM/zip loader, the controller
assignment system, the display planner, the test card, the intro animation
and its synthesised sound, and the whole frontend.

Never used (by decision): RetroArch, libretro cores or any other external
emulator, as a library or as a hidden backend. Musashi was evaluated and not
used (its SoftFloat part has licence terms that do not fit GPL-3.0).
