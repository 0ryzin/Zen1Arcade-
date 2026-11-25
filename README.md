# Zen1 Arcade

Zen1 Arcade is a Linux arcade emulator platform built for one job: turning an
arcade cabinet into a dedicated arcade machine. It has its own emulation engine,
its own frontend, its own control system and its own identity.

> **Status: version 0.6 in development.** Zen1's own engine emulates six
> arcade systems: Capcom CPS-1 (136 sets including regional revisions),
> **Capcom CPS-2 (291 sets)**, Capcom 1942 (4), Namco Pac-Man (3), Namco
> Galaxian (5) and SNK Neo Geo MVS (122); and, new in 0.6, **home consoles
> and handhelds**: NES/Famicom, SNES/Super Famicom, Game Boy, Game Boy
> Color, Master System, Game Gear, SG-1000, Mega Drive/Genesis and PC
> Engine/TurboGrafx-16 (HuCards), each a Zen1 machine module around an
> open-source core compiled into the engine. Full list and what has been
> verified: [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md). The frontend has
> one start-up scene (intro settling into INSERT COIN), original menu music,
> one artwork library with automatic artwork, a detected-controller list
> with per-action remapping, three-level volume on the 1P/2P buttons and a
> smart display fit for the 5:4 cabinet screen.
> Checked on the build server only, with test and homebrew cartridges:
> **no commercial game has been verified yet, and nothing has been tested
> on the Steam Deck or cabinet.** Install and test:
> [docs/STEAM_DECK.md](docs/STEAM_DECK.md).

## What Zen1 Arcade is (and is not)

- **Is:** a frontend, a library, a control system **and** an emulation engine
  with machine modules for specific arcade hardware, all under one architecture.
- **Is not:** a skin over RetroArch or any other emulator. No other emulator's
  menus, branding or UI appear, and no other emulator runs hidden underneath.
- **First hardware target:** Capcom CP System (CPS1), the hardware behind
  *Street Fighter II*. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for why.
- **Consoles:** cartridges (`.nes .sfc .smc .gb .gbc .sms .gg .sg .md .gen
  .pce`, or a `.zip` holding one) anywhere in `Games` appear in the same
  library. `.bin` Mega Drive files are recognised inside a folder named for
  the system (for example `Games/Mega Drive`).

## Documents

| Document | Contents |
| --- | --- |
| [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) | Every supported game, its controls, and whether it has been tested |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Process model, engine design, modules, first hardware target, what we write vs. what we reuse |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phases 1–8 with exit gates |
| [docs/CONTROLS.md](docs/CONTROLS.md) | Player 1/2 identification, seven-button layout, remapping |
| [docs/DISPLAY.md](docs/DISPLAY.md) | 1280 × 1024 presentation, aspect ratio, bezels, safe recovery |
| [docs/STORAGE.md](docs/STORAGE.md) | The permanent `~/Zen1Arcade` data folder |
| [docs/LICENSING.md](docs/LICENSING.md) | GPL-3.0-or-later and third-party rules |
| [docs/RESEARCH.md](docs/RESEARCH.md) | Projects and components researched, with sources |
| [docs/HARDWARE_RESEARCH.md](docs/HARDWARE_RESEARCH.md) | Arcade platforms and consoles: what each needs, in what order, and the sf2ce investigation |
| [docs/SWITCH.md](docs/SWITCH.md) | Nintendo Switch (NSP/XCI) investigation |
| [THIRD_PARTY.md](THIRD_PARTY.md) | Third-party components and their licences |

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE) and
[docs/LICENSING.md](docs/LICENSING.md).

Zen1 runs only hardware its own engine implements. It never launches other
emulators and never loads emulator plug-ins (such as libretro cores) at run
time. Console support compiles open-source emulation cores into Zen1's own
engine, each under its own GPL-compatible licence
([THIRD_PARTY.md](THIRD_PARTY.md)).

## Games and BIOS files

Zen1 Arcade does not include, download or distribute games, ROMs or BIOS files.
You supply files you legally own or are authorised to use.

## Independence

Zen1 Arcade is a standalone project. It shares no code, branding, settings,
libraries or repositories with any other project of the owner's (including
Nexus).
