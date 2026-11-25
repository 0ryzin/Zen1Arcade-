# Third-party components

Everything below is compatible with Zen1 Arcade's GPL-3.0-or-later licence.
Each vendored component keeps its own licence file next to its code.

| Component | Purpose | Licence | How it is used | Where |
| --- | --- | --- | --- | --- |
| Moira (Dirk W. Hoffmann) | Motorola 68000 CPU | MIT | Vendored, static | `third_party/moira` (+ `VERSION`, `LICENSE`) |
| z80 (superzazu) | Zilog Z80 CPU | MIT | Vendored, static | `third_party/z80` |
| ymfm (Aaron Giles) | Yamaha YM2151 and YM2610 (FM, SSG, ADPCM) sound; its SSG engine also plays 1942's AY-3-8910s | BSD-3-Clause | Vendored, static | `third_party/ymfm` |
| MAME CPS1 driver (Paul Leaman and MAME contributors) | CPS1 memory map, board data, video rules: **adapted** into Zen1 code | BSD-3-Clause | Rewritten/adapted source in `machines/cps1` | `machines/cps1/LICENSE-MAME-BSD-3-Clause.txt` |
| MAME Pac-Man driver (Nicola Salmoria, Stephane Humbert, David Widel) | Pac-Man memory map, tile/sprite layout, Ms. Pac-Man decoder: **adapted** into Zen1 code | BSD-3-Clause | Rewritten/adapted source in `machines/pacman` | `machines/cps1/LICENSE-MAME-BSD-3-Clause.txt` (same licence text) |
| MAME Galaxian driver (Aaron Giles and MAME contributors) | Galaxian memory map, video (column scroll, stars, bullets), ROM sets: **adapted** | BSD-3-Clause | Rewritten/adapted source in `machines/galaxian` | `machines/cps1/LICENSE-MAME-BSD-3-Clause.txt` (same licence text) |
| MAME Neo Geo driver and sprite/video code (Bryan McPhail, Ernesto Corvi, Andrew Prime, Zsolt Vasvari, David Haywood and MAME contributors) | Neo Geo memory map, LSPC timing, sprite zoom rules, BIOS and game ROM lists (`tools/gen_neogeo_games.py` reads MAME's `neogeo.cpp`): **adapted** | BSD-3-Clause | Rewritten/adapted source in `machines/neogeo` | `machines/cps1/LICENSE-MAME-BSD-3-Clause.txt` (same licence text) |
| MAME CPS-2 driver and decryption (Paul Leaman, Andreas Naive, Nicola Salmoria, Charles MacDonald) | CPS-2 memory map, sprites and priorities, raster interrupts, ROM lists (`tools/gen_cps2_games.py`); the decryption algorithm and s-box tables are **ported** (`machines/cps1/cps2crypt.cpp`) | BSD-3-Clause | `machines/cps1/cps2.cpp`, `cps2crypt.cpp`, `cps1_video.cpp` | `machines/cps1/LICENSE-MAME-BSD-3-Clause.txt` |
| MAME QSound HLE (superctr, Valley Bell) | QSound DSP high-level emulation, **ported** | BSD-3-Clause | `machines/cps1/qsound.cpp` | `machines/cps1/LICENSE-MAME-BSD-3-Clause.txt` |
| MAME 1942 driver (Paul Leaman, Couriersud) | 1942 memory maps, interrupts, graphics layouts, sprite engine, ROM lists: **adapted** | BSD-3-Clause | `machines/c1942` | `machines/cps1/LICENSE-MAME-BSD-3-Clause.txt` |
| Press Start 2P (CodeMan38) | Pixel font for headings, labels, clock | SIL OFL 1.1 | Embedded in the frontend | `apps/frontend/fonts/OFL-PressStart2P.txt` |
| VT323 (Peter Hull) | Pixel terminal font for details | SIL OFL 1.1 | Embedded | `apps/frontend/fonts/OFL-VT323.txt` |
| Pixelify Sans (The Pixelify Sans Project Authors) | Pixel font for game titles | SIL OFL 1.1 | Embedded | `apps/frontend/fonts/OFL-PixelifySans.txt` |
| SDL3 | Engine window, rendering, audio, game input; intro sound in the frontend | zlib | Dynamic (bundled in the AppImage) | system / AppImage |
| Qt 6 (Core, Gui, Qml, Quick) | Frontend | LGPL-3.0 | Dynamic (bundled in the AppImage) | system / AppImage |
| zlib | Reading .zip ROM sets | zlib | Dynamic | system |

Written by Zen1 itself (no third-party code): the Namco WSG sound chip, the
OKI MSM6295 ADPCM sound chip, the CPS1 renderer and machine glue, the ROM/zip loader, the controller
assignment system, the display planner, the test card, the intro animation
and all synthesised sounds, joystick calibration, and the whole frontend.

Game artwork is not included. If artwork download is switched on, Zen1
fetches pictures for the games you have from a public arcade database
(Arcade Database, adb.arcadeitalia.net) and keeps them in
`~/Zen1Arcade/Artwork/downloaded` for your own use. The source can be changed
in `Configurations/settings.toml` (`artwork.source`).

Never used (by decision): RetroArch, libretro cores or any other external
emulator, as a library or as a hidden backend. Musashi was evaluated and not
used (its SoftFloat part has licence terms that do not fit GPL-3.0).
