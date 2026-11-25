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
