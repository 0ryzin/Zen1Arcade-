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
| Gearboy (Ignacio Sanchez) | Game Boy and Game Boy Color emulation core | GPL-3.0 | Vendored, compiled into the engine (`machines/consoles/gb.cpp` wraps it) | `third_party/gearboy` (+ `VERSION`, `LICENSE`; bundles miniz, MIT) |
| Gearsystem (Ignacio Sanchez) | Master System, Game Gear and SG-1000 emulation core | GPL-3.0 | Vendored, compiled into the engine (`machines/consoles/sega8.cpp`) | `third_party/gearsystem` (+ `VERSION`, `LICENSE`; bundles miniz, MIT, and emu2413, MIT) |
| Geargrafx (Ignacio Sanchez) | PC Engine / TurboGrafx-16 HuCard emulation core | GPL-3.0 | Vendored, compiled into the engine (`machines/consoles/pce.cpp`) | `third_party/geargrafx` (+ `VERSION`, `LICENSE`); its media layer's libraries are vendored alongside: libchdr (BSD-3-Clause), LZMA SDK (public domain), minivorbis (BSD-3-Clause), miniz (MIT), qon (MIT), zstd decoder (BSD-3-Clause, dual GPL-2), nlohmann json (MIT) |
| Nes_Emu / QuickNES (Shay Green; libretro maintainers) | NES / Famicom emulation core | LGPL-2.1-or-later (the repository's own licence text is GPL-2.0 with no version limit) | Vendored `nes_emu/` only, compiled into the engine (`machines/consoles/nes.cpp`); the libretro wrapper is not used | `third_party/quicknes` (+ `VERSION`, `LICENSE`) |
| LakeSnes (angelo_wf and contributors) | SNES / Super Famicom emulation core | MIT | Vendored `snes/` only, compiled into the engine (`machines/consoles/snes.cpp`) | `third_party/lakesnes` (+ `VERSION`, `LICENSE.txt`) |
| clownmdemu-core (Clownacy), with clown68000, clownz80, clowncommon | Mega Drive / Genesis emulation core | AGPL-3.0-or-later (clowncommon: 0BSD-style) | Vendored, compiled into the engine (`machines/consoles/md.cpp`). GPL-3.0 section 13 allows combining GPL-3.0 and AGPL-3.0 code; this core's code stays under the AGPL-3.0, whose extra network-use term does not arise in Zen1 (no network play). Its built-in Mega CD boot ROM is Clownacy's own open-source replacement (clownmdemu-mcd-boot), not Sega firmware; Zen1 does not offer Mega CD. | `third_party/clownmdemu` (+ `VERSION`, `LICENCE.txt`) |
| ares (Near and the ares team) | Atari 2600, SNES with enhancement chips (Super FX, SA-1, DSP-n, Cx4, S-DD1, SPC7110, ST01x, OBC1), Sega 32X, PC Engine CD, Nintendo 64 | ISC | Vendored, built headless by its own CMake and linked into the engine as one isolated object (`machines/ares`); Zen1's host replaces ares's own UI. Bundled with it: nall and libco (ISC), mia (ISC; its game databases are installed next to `zen1-engine`), paraLLEl-RDP with volk and the Vulkan headers (MIT; Apache-2.0 for the headers), sljit (BSD-2-Clause), libchdr (BSD-3-Clause) with zstd (BSD-3-Clause) and miniz (MIT), TZXFile (MIT), qon (MIT). **ares's firmware dumps are not distributed**: each is replaced by a one-byte placeholder and the user's own file is read from `~/Zen1Arcade/BIOS`. The one exception is the SNES SPC700 IPL (64 bytes), which every SNES emulator embeds (LakeSnes, above, carries the same table). | `third_party/ares` (+ `LICENSE`, `VERSION.zen1` with Zen1's changes) |
| mGBA (Vicki Pfau and contributors) | Game Boy Advance emulation core, with its own high-level BIOS | MPL-2.0 (GPL-compatible) | Vendored, built as a static library by its own CMake (no frontends), isolated with Zen1's wrapper (`machines/consoles/gba.cpp`). Bundles inih (BSD-3-Clause). | `third_party/mgba` (+ `LICENSE`, `VERSION.zen1`) |
| ProSystem (Greg Stanton; libretro maintainers), with BupBoop/CoreTone (Osman Celimli) | Atari 7800 emulation core (MARIA, TIA, POKEY, cartridge database) | GPL-2.0-or-later (CoreTone: zlib-style) | Vendored `core/` and `bupboop/`; its libretro front end is not used: `machines/consoles/a7800.cpp` drives it, and a few libretro-common headers (MIT) plus Zen1's own stdio file functions stand in for libretro-common | `third_party/prosystem` (+ `License.txt`, `VERSION.zen1`) |
| Press Start 2P (CodeMan38) | Pixel font for headings, labels, clock | SIL OFL 1.1 | Embedded in the frontend | `apps/frontend/fonts/OFL-PressStart2P.txt` |
| VT323 (Peter Hull) | Pixel terminal font for details | SIL OFL 1.1 | Embedded | `apps/frontend/fonts/OFL-VT323.txt` |
| Pixelify Sans (The Pixelify Sans Project Authors) | Pixel font for game titles | SIL OFL 1.1 | Embedded | `apps/frontend/fonts/OFL-PixelifySans.txt` |
| SDL3 | Engine window, rendering, audio, game input; intro sound in the frontend | zlib | Dynamic (bundled in the AppImage) | system / AppImage |
| Qt 6 (Core, Gui, Qml, Quick) | Frontend | LGPL-3.0 | Dynamic (bundled in the AppImage) | system / AppImage |
| zlib | Reading .zip ROM sets | zlib | Dynamic | system |

Written by Zen1 itself (no third-party code): the console machine modules
around the cores (panel mapping, picture and sound handling, the Mega Drive
audio mixer), the generated console test cartridges, the Namco WSG sound chip, the
OKI MSM6295 ADPCM sound chip, the CPS1 renderer and machine glue, the ROM/zip loader, the controller
assignment system, the display planner, the test card, the intro animation
and all synthesised sounds, joystick calibration, and the whole frontend.

Game artwork is not included. If artwork download is switched on, Zen1
fetches pictures for the games you have from a public arcade database
(Arcade Database, adb.arcadeitalia.net) and keeps them in
`~/Zen1Arcade/Artwork/downloaded` for your own use. For console games the
pictures come from the libretro-thumbnails collections on GitHub, looked up
by the cartridge's No-Intro file name. The sources can be changed in
`Configurations/settings.toml` (`artwork.source`, `artwork.console_source`).

Never used (by decision): RetroArch, the libretro API, or any other
emulator as an external program or hidden backend. The console cores above
are emulation libraries whose source is compiled into Zen1's own engine and
driven by Zen1's machine modules (their own front ends, debuggers and
libretro wrappers are not used). Each core is linked into one object whose
internal symbols are made local, so cores that share class names coexist
(see `machines/consoles/CMakeLists.txt`).

Evaluated for consoles and not used because their licences do not fit
GPL-3.0: Genesis Plus GX and Snes9x (non-commercial terms), PicoDrive
(MAME-style non-commercial licence). Musashi was evaluated and not
used (its SoftFloat part has licence terms that do not fit GPL-3.0).
