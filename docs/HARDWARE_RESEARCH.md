# Arcade hardware: what to emulate next

This note covers which arcade platforms Zen1's own engine should add, in
what order, and why. Every module is Zen1 code inside `machines/`. Where
MAME (BSD-3-Clause drivers) or other permissively licensed code is used as
a reference or adapted, it is credited in THIRD_PARTY.md. Zen1 never loads
cores or runs another emulator.

## How Zen1 is organised (and how MAME does it)

MAME splits emulation into *devices* (CPUs, sound chips, video chips) and
*drivers* (one per board family) that wire them together with an address
map, a ROM list per game, input ports and timing. Zen1 follows the same
idea, more simply:

| Zen1 piece | Role | Example |
| --- | --- | --- |
| `third_party/` | CPU and sound cores with GPL-compatible licences | Moira 68000 (MIT), z80 (MIT), ymfm (BSD-3) |
| `machines/<board>/` | One module per board family: memory map, video, own sound chips, game list | `cps1`, `pacman` |
| `machines/registry.cpp` | The engine-wide list of supported games | |
| `libs/emu` | ROM loading (zip, CRC checks, parent sets, interleaving), the `Machine` interface, recognised-but-unsupported games | |
| `apps/engine` | Window, timing, audio, input, display plan, hold shortcuts | |

ROM sets follow **current MAME naming**: one `.zip` per game, files checked
by CRC, clones needing their parent zip (for example `pacman.zip` +
`puckman.zip`). `.7z` sets are recognised but not read yet (that needs an
LZMA decoder; planned). BIOS-based systems (Neo Geo) need a BIOS zip
(`neogeo.zip`) in `~/Zen1Arcade/Games` or `~/Zen1Arcade/BIOS`. Zen1 uses
the first MVS BIOS it finds in it, in this order: `sp-s2.sp1` (Asia/Europe
MVS v2), `sp-s.sp1`, `sp-u2.sp1`, `sp-e.sp1`, `sp1-u2`, `vs-bios.rom`,
`sp-j2.sp1`, `sp-s3.sp1`, `sp-45.sp1`, then Universe BIOS 4.0 / 3.3. The
fix-layer BIOS (`sfix.sfix`), sound BIOS (`sm1.sm1`) and zoom table
(`000-lo.lo`) also come from `neogeo.zip`. Settings and high scores (the
MVS backup RAM) are saved in `~/Zen1Arcade/Saves/<set>.nv`.

## Platforms ranked for Zen1

Ranking: desirable games per unit of work, how hard it is to emulate
accurately, and whether the Steam Deck can run it at full speed (all of
these can).

| # | Platform | Key hardware | What Zen1 already has | Games it unlocks | Effort |
| --- | --- | --- | --- | --- | --- |
| done | **Capcom CPS-1** | 68000, Z80, YM2151, OKI6295, CPS-A/B | everything | 21 games (SF2 family, Final Fight, Strider, Ghouls'n Ghosts, Knights, Captain Commando...) | - |
| done | **Namco Pac-Man** | Z80, tilemap + sprites, Namco WSG | everything | Pac-Man, Puck Man, Ms. Pac-Man | - |
| 1 | CPS-1 QSound (CPS 1.5) | + Kabuki-encrypted Z80, QSound DSP16A | 68000/Z80/video | Cadillacs and Dinosaurs, The Punisher, Warriors of Fate, Saturday Night Slam Masters | Medium: QSound DSP (HLE possible) and Kabuki decryption |
| done | **Namco Galaxian** | Z80, column-scroll tilemap, stars, bullets, analogue sound | everything (sound approximated) | Galaxian and 4 sets (Midway, Taito, Super Galaxians) | - |
| done | **SNK Neo Geo MVS** (unencrypted carts) | 68000, Z80, YM2610, LSPC2 sprites, BIOS | everything; calendar simplified, no memory card | 122 games: Metal Slug 1/2, KOF '94-'97, Samurai Shodown 1-4, Neo Bomberman, Blazing Star... | - |
| done | **Capcom CPS-2** | encrypted 68000, CPS-A/B, CPS-2 sprites, QSound, EEPROM | everything (QSound exact with qsound.zip) | 291 sets: SSF2T, SF Alpha 1-3, Darkstalkers, Marvel, X-Men, D&D, 19XX, 1944, Progear... | - |
| done | **Capcom 1942** | 2x Z80, 2x AY-3-8910 | everything (AY via ymfm SSG) | 1942 and clones | - |
| 2 | Neo Geo encrypted carts (CMC42/CMC50, PCM2, SMA) | as above + cartridge decryption | Neo Geo | Metal Slug X/3/4/5, KOF '98-2003, Garou | Medium: decryption is algorithmic, documented in MAME (BSD-3) |
| 3 | Namco Galaga | 3x Z80, 05xx stars, 51xx/54xx customs | Z80, Galaxian video ideas | Galaga, Dig Dug | Medium: customs are simple MCUs |
| 5 | Konami 68000 boards (TMNT, TMNT2/Sunset Riders, Simpsons) | 68000 / Konami 6809 variant, K052109/K051960/K053245/K053251 video, K007232/K053260 sound | 68000, Z80, YM2151 | TMNT, Turtles in Time, Sunset Riders, The Simpsons, X-Men | High: many custom video chips, but shared across games |
| 6 | Capcom Z80/6809 era | Z80 or 6809, AY-3-8910 / YM2203 | Z80, ymfm (YM2203) | 1942, Gun.Smoke, Commando, Ghosts'n Goblins | Low-medium: needs a 6809 core |
| 7 | Nintendo Donkey Kong / Midway 8080 | Z80 or 8080, i8035 sound CPU, analogue sound | Z80 | Donkey Kong, Space Invaders | Medium: 8035 core plus analogue circuits (or samples) |
| 8 | Technos, Irem M72, Seibu | 6309/63701, V30, YM2151/YM3812 | ymfm | Double Dragon, R-Type, Raiden | Medium: 6309 and V30 cores |
| 9 | Midway Y/T-Unit | TMS34010, Williams/DCS sound (ADSP-2105) | none | Mortal Kombat 1-3 | High: TMS34010 and ADSP-2105 cores |
| later | Killer Instinct | MIPS R4600, 3dfx Voodoo, hard disk (CHD) | none | Killer Instinct 1/2 | Very high; CHD image format |

**Next, in order:** CPS-1 QSound boards (Cadillacs and Dinosaurs, The
Punisher, Warriors of Fate: QSound and the CPS-2 code are now in Zen1; the
Kabuki Z80 decryption is what remains), Neo Geo encrypted carts, 1943 /
Commando / Gun.Smoke (same Capcom Z80 family as 1942, YM2203 in ymfm), then
Galaga (needs the Namco 5xxx custom MCUs: an MB88 core), Donkey Kong
(i8035 sound CPU), Sega System 16 (Golden Axe, Altered Beast: 68000 + FD1094
decryption on some sets), Konami (TMNT, Simpsons). Earlier order was:
CPS-1 QSound, then Neo Geo, then Galaga/Galaxian, then
CPS-2. That covers most of the target list (Cadillacs and Dinosaurs, Metal
Slug 1 and 2, Galaga, Dig Dug, Galaxian, Frogger) with the cores Zen1
already has. The Konami beat-'em-ups come after that.

## Target games

| Game | MAME set | Hardware | Players | Joystick | Buttons | BIOS | In Zen1 now | Missing |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Pac-Man | pacman (+puckman) | Namco Pac-Man | 1-2 alt. | 4-way | 0 | - | **Supported** | - |
| Ms. Pac-Man | mspacman | Namco Pac-Man + aux board | 1-2 alt. | 4-way | 0 | - | **Supported** | - |
| Street Fighter II / CE | sf2, sf2ce | CPS-1 | 2 | 8-way | 6 | - | **Supported** | - |
| SF II' Hyper Fighting ("Turbo") | sf2hf (USA: sf2t) | CPS-1 | 2 | 8-way | 6 | - | **Supported** (World set) | sf2t: use sf2hf.zip |
| Final Fight | ffight | CPS-1 | 2 | 8-way | 2 | - | **Supported** | - |
| Galaga | galaga | Namco Galaga | 1-2 alt. | 2-way | 1 | - | No | 3x Z80 board, 05xx/51xx/54xx |
| Dig Dug | digdug | Namco Galaga | 1-2 alt. | 4-way | 1 | - | No | as Galaga + 53xx |
| Galaxian | galaxian | Namco Galaxian | 1-2 alt. | 2-way | 1 | - | **Supported** | sound approximated |
| Frogger | frogger | Konami (Galaxian-based) | 1-2 alt. | 4-way | 0 | - | No | AY-3-8910 sound board |
| Donkey Kong | dkong | Nintendo | 1-2 alt. | 4-way | 1 | - | No | i8035, analogue sound |
| Space Invaders | invaders | Midway 8080 | 1-2 alt. | 2-way | 1 | - | No | 8080, analogue sound |
| 1942 | 1942 | Capcom | 1-2 alt. | 8-way | 2 | - | **Supported** | sound filter approximated |
| BurgerTime | btime | Data East | 1-2 alt. | 4-way | 1 | - | No | 6502 x2, AY-3-8910 |
| Ghosts'n Goblins | gng | Capcom | 1-2 alt. | 8-way | 2 | - | No | 6809, YM2203 |
| Gun.Smoke | gunsmoke | Capcom | 1-2 | 8-way | 3 | - | No | YM2203 board |
| Mortal Kombat | mk | Midway Y-Unit | 2 | 8-way | 5 | - | No | TMS34010, Williams sound |
| Mortal Kombat II | mk2 | Midway T-Unit | 2 | 8-way | 5 | - | No | TMS34010, DCS |
| Killer Instinct | kinst | Midway/Rare | 2 | 8-way | 6 | - | No | R4600, Voodoo, CHD |
| Double Dragon | ddragon | Technos | 2 | 8-way | 3 | - | No | 6309, 63701, MSM5205 |
| TMNT | tmnt | Konami | 4 | 8-way | 2 | - | No | Konami video/sound customs |
| Turtles in Time | tmnt2 | Konami | 4 | 8-way | 2 | - | No | Konami video/sound customs |
| Cadillacs and Dinosaurs | dino | CPS-1 QSound | 3 | 8-way | 3 | - | No | QSound, Kabuki Z80 |
| The Simpsons | simpsons | Konami | 4 | 8-way | 2 | - | No | Konami 053248 CPU |
| Streets of Rage | (console game) | Mega Drive; arcade only as Mega-Tech `mt_srage` | 2 | 8-way | 3 | megatech | No | Mega Drive hardware |
| Metal Slug / 2 | mslug, mslug2 | Neo Geo MVS | 2 | 8-way | 3 | neogeo.zip | **Supported** | - |
| Metal Slug X / 3 | mslugx, mslug3 | Neo Geo MVS | 2 | 8-way | 3 | neogeo.zip | No | protection / encryption |
| Contra | contra | Konami | 2 | 8-way | 2 | - | No | 6309, K007121 |
| Sunset Riders | ssriders | Konami | 4 | 8-way | 2 | - | No | Konami customs |
| Raiden | raiden | Seibu | 2 | 8-way | 2 | - | No | V30 x2, Seibu sound |
| Gradius | nemesis / gradius | Konami Nemesis | 2 | 8-way | 3 | - | No | Nemesis video, K005289 |
| R-Type | rtype | Irem M72 | 2 | 8-way | 2 | - | No | V30, M72 video |

Unsupported games that Zen1 recognises (this table, in
`libs/emu/src/known_games.cpp`) show in the library with their real details,
marked NOT SUPPORTED, together with what is missing.

## Licences of reference projects

| Project | Licence | Use in Zen1 |
| --- | --- | --- |
| MAME drivers used so far (CPS-1, Pac-Man) | BSD-3-Clause (per file) | Hardware data and rules adapted, with attribution |
| MAME as a whole | GPL-2.0-or-later (some files BSD-3) | Reference only; per-file licences are checked before adapting |
| ymfm (Yamaha FM incl. YM2610/YM2203) | BSD-3-Clause | Vendored |
| Moira (68000) | MIT | Vendored |
| FBNeo | Non-commercial licence | **Not used**: incompatible with GPL-3.0 |
| RetroArch / libretro cores | various | **Not used**: by project decision |

## Home consoles (0.6)

Zen1 0.6 adds home consoles and handhelds as machine modules of its own
engine, each around an open-source emulation core compiled into Zen1 (the
cores' own front ends and libretro wrappers are not used, and no other
emulator is ever started). Writing every console from scratch was not
needed: well-tested cores with GPL-compatible licences exist, and Zen1's
job is the cabinet around them (panel mapping, display fit, saves, library,
artwork). Order: the 8-bit and 16-bit systems with small, clean cores first.

| System | Core chosen | Licence | Why this core | Alternatives considered |
|---|---|---|---|---|
| NES / Famicom | QuickNES (Nes_Emu) | LGPL-2.1+ | Small, fast, ~70 mappers, no dependencies | Mesen2 (GPL-3, more accurate, much larger: a later upgrade), Nestopia UE (GPL-2+) |
| SNES / Super Famicom | LakeSnes | MIT | Small C core, clean API | bsnes (GPL-3, accurate and heavy), Snes9x (non-commercial licence: not usable) |
| Game Boy / Color | Gearboy | GPL-3 | Accurate, many mappers, same family as the next two | SameBoy (MIT, larger), Gambatte (GPL-2) |
| Master System / Game Gear / SG-1000 | Gearsystem | GPL-3 | Accurate, includes YM2413 FM | Genesis Plus GX (non-commercial: not usable) |
| PC Engine / TurboGrafx-16 | Geargrafx | GPL-3 | Covers the whole HuCard library | Mednafen (GPL-2, a full emulator, not a library) |
| Mega Drive / Genesis | clownmdemu | AGPL-3+ | Clean C library with callbacks; combinable with GPL-3 (section 13) | Genesis Plus GX and PicoDrive (non-commercial: not usable), BlastEm (GPL-3, x86 JIT, built as a program) |

Researched and **not implemented yet**:

| System | Status | Why not yet | Path |
|---|---|---|---|
| Game Boy Advance | Files recognised and explained | A larger core; wanted the 8/16-bit systems first | mGBA (MPL-2.0, GPL-compatible) as a module |
| Nintendo 64 | Files recognised and explained | Needs a 3D renderer (Vulkan/GL), dynarec and a much larger core; would not fit the engine's simple frame model | mupen64plus family (GPL-2+), a separate project |
| Atari 2600 | Files recognised and explained | Small library on a cabinet; lower priority | Stella's core (GPL-2.0-or-later) |
| Atari 7800 | Files recognised and explained | As above | ProSystem (GPL-2.0) |
| Sega 32X, Mega CD | 32X files recognised and explained | clownmdemu has no 32X; Mega CD needs CD images and a BIOS choice | later |
| PC Engine CD | Not offered | Needs a System Card BIOS (proprietary, not included) | Geargrafx supports it once a user BIOS is handled |
| PlayStation and later | Out of scope (by request) | | |

Testing uses freely available test and homebrew cartridges, fetched from
their own repositories at test time and never bundled: blargg's NES and
Game Boy test ROMs, Peter Lemon's NES/SNES/GB/SMS/PCE programs, dshadoff's
PC Engine controller test and SGDK's Mega Drive samples, plus two small
cartridges Zen1 generates itself (`tools/make_test_roms.py`: Game Gear and
Game Boy Color colour tests). See `tools/console_smoke.sh` and
`docs/COMPATIBILITY.md` for what each one showed.

## Street Fighter II': Champion Edition (`sf2ce.zip`) "missing files"

Investigated in 0.6. The zip was not at fault; Zen1's ROM-set data was:

1. **Wrong parent.** Zen1 treated `sf2ce` as a clone of `sf2`. In MAME,
   `sf2ce` (and `sf2hf`) are parent sets with their own program and graphics
   naming, so a correct non-merged or split `sf2ce.zip` is complete on its
   own and must not need `sf2.zip`.
2. **One revision only.** Zen1 knew only the World 920513 program. A zip
   from another MAME version or a regional revision (for example the USA
   920313 program, MAME `sf2ceua`) failed every CRC check and was reported
   as missing files, although the board is identical.
3. **Shared files elsewhere.** Files shared with another set were looked
   for only in the parent zip.

Fixes: the CPS-1 list is regenerated from MAME's driver with MAME's own
parents and every clone revision that runs on the same board (136 sets);
the generator now refuses a curated parent that differs from MAME's. A zip
is identified by the CRCs it holds (any file names), so an `sf2ce.zip` that
holds the USA revision runs as `sf2ceua`; a file can come from any zip in
`Games` or `BIOS`; and when a parent zip is absent the message names it.
Unit test: "ROM identification" in `tests/test_cps1.cpp` (zips made in the
test whose files have the real sets' CRCs).
