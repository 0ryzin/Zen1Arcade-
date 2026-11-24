# Zen1 Arcade

Zen1 Arcade is a Linux arcade emulator platform built for one job: turning an
arcade cabinet into a dedicated arcade machine. It has its own emulation engine,
its own frontend, its own control system and its own identity.

> **Status: planning (pre-Phase 2).** No application or emulator code exists
> yet. This repository currently contains the agreed direction and architecture
> documents only. Nothing described here is implemented or tested unless a
> document says so explicitly.

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
| [docs/LICENSING.md](docs/LICENSING.md) | MIT vs GPL-3.0 for this project (undecided) and third-party rules |
| [docs/RESEARCH.md](docs/RESEARCH.md) | Projects and components researched, with sources |
| [THIRD_PARTY.md](THIRD_PARTY.md) | Proposed third-party components and their licences |

## Licence

**Not chosen yet.** The project owner is deciding between MIT and GPL-3.0; see
[docs/LICENSING.md](docs/LICENSING.md). Until a licence is added, all rights are
reserved by the owner.

## Games and BIOS files

Zen1 Arcade does not include, download or distribute games, ROMs or BIOS files.
You supply files you legally own or are authorised to use.

## Independence

Zen1 Arcade is a standalone project. It shares no code, branding, settings,
libraries or repositories with any other project of the owner's (including
Nexus).
