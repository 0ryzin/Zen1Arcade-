# Zen1 Arcade

Zen1 Arcade is a Linux arcade emulator platform built for one job: turning an
arcade cabinet into a dedicated arcade machine. It has its own emulation engine,
its own frontend, its own control system and its own identity.

> **Status: version 0.3 in development.** Zen1's own engine emulates Capcom
> CPS-1 (21 games) and Namco Pac-Man hardware (Pac-Man, Puck Man, Ms. Pac-Man).
> The frontend has the start-up animation, INSERT COIN, one artwork library,
> SETUP, controller assignment, a controller test and joystick calibration.
> Checked on the build server only: **no real game has been verified yet,
> and nothing has been tested on the Steam Deck or cabinet.** Install and
> test: [docs/STEAM_DECK.md](docs/STEAM_DECK.md). Next hardware:
> [docs/HARDWARE_RESEARCH.md](docs/HARDWARE_RESEARCH.md).

## Building

Linux, CMake ≥ 3.22, a C++20 compiler, Qt 6.2+ (Quick) and SDL 3.2+.

```
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/sdl3
cmake --build build
ctest --test-dir build
```

| Program | What it is |
| --- | --- |
| `zen1-arcade` | Frontend (Qt Quick) |
| `zen1-engine` | Engine (SDL3): runs games (`--game SET`), the test card, `--scan` / `--list-games` for the library |
| `zen1-diag` | Read-only hardware report; build with `-DZEN1_STATIC_DIAG=ON` for a portable binary |

GitHub Actions builds all three, runs the tests and produces an AppImage.

## Target setup

| Item | Target |
| --- | --- |
| Operating system | Linux only. SteamOS Desktop Mode on Steam Deck is the primary target |
| Hardware | Steam Deck in its dock, connected to an Arcade1Up cabinet |
| Display | Cabinet screen at 1280 × 1024 (5:4) |
| Controls | Two USB control panels (named "Player 1" and "Player 2"), each with an 8-way stick, six action buttons and one player/start button |
| Launch | Started by the user from Desktop Mode. Zen1 Arcade never takes over boot |

## What Zen1 Arcade is (and is not)

- **Is:** a frontend, a library, a control system **and** an emulation engine
  with machine modules for specific arcade hardware, all under one architecture.
- **Is not:** a skin over RetroArch or any other emulator. No other emulator's
  menus, branding or UI appear, and no other emulator runs hidden underneath.
- **First hardware target:** Capcom CP System (CPS1), the hardware behind
  *Street Fighter II*. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for why.

## Documents

| Document | Contents |
| --- | --- |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Process model, engine design, modules, first hardware target, what we write vs. what we reuse |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phases 1–8 with exit gates |
| [docs/CONTROLS.md](docs/CONTROLS.md) | Player 1/2 identification, seven-button layout, remapping |
| [docs/DISPLAY.md](docs/DISPLAY.md) | 1280 × 1024 presentation, aspect ratio, bezels, safe recovery |
| [docs/STORAGE.md](docs/STORAGE.md) | The permanent `~/Zen1Arcade` data folder |
| [docs/LICENSING.md](docs/LICENSING.md) | GPL-3.0-or-later and third-party rules |
| [docs/RESEARCH.md](docs/RESEARCH.md) | Projects and components researched, with sources |
| [docs/HARDWARE_RESEARCH.md](docs/HARDWARE_RESEARCH.md) | Arcade platforms and target games: what each needs, in what order |
| [docs/SWITCH.md](docs/SWITCH.md) | Nintendo Switch (NSP/XCI) investigation |
| [THIRD_PARTY.md](THIRD_PARTY.md) | Third-party components and their licences |

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE) and
[docs/LICENSING.md](docs/LICENSING.md).

Zen1 runs only hardware its own engine implements. It never loads
third-party emulator cores or launches other emulators.

## Games and BIOS files

Zen1 Arcade does not include, download or distribute games, ROMs or BIOS files.
You supply files you legally own or are authorised to use.

## Independence

Zen1 Arcade is a standalone project. It shares no code, branding, settings,
libraries or repositories with any other project of the owner's (including
Nexus).
