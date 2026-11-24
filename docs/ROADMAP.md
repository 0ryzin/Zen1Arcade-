# Zen1 Arcade — Roadmap

Each phase ends with a build that runs and a **gate** that must pass before
the next phase starts. "Tested on the cabinet" is only claimed after the
owner has run the hardware checklist on the real Steam Deck + cabinet.

| Phase | Scope | Gate |
| --- | --- | --- |
| 1 · Research & architecture | Done; revised for Zen1's own engine (this repository's docs) | Owner approves architecture and answers open decisions |
| 2 · Foundation | CMake project, CI, AppImage build; frontend shell that opens fullscreen on the chosen output; `~/Zen1Arcade` creation; settings; display detection report; input device diagnostic (`--list-input-devices`); engine process skeleton + IPC | On the Deck: app opens fullscreen on the 1280 × 1024 cabinet screen; diagnostic shows both panels with their names and identities |
| 3 · Controller systems | Player 1/2 identification (saved binding → name → wizard), seven-button layout, coin handling, test screen, remapping, swap, reset, reconnect, menu navigation | 5 reboots and replugs with no P1/P2 swap; all 11 inputs per player (4 directions, 6 buttons, player button) register correctly |
| 4 · Engine | See milestones below | *Street Fighter II* (CPS1) plays correctly with sound for both players |
| 5 · Frontend | Zen1 Arcade logo and intro, game grid, details, transitions, favourites, recently played, collections, search | Grid holds 60 fps; everything usable with Player 1's panel only. **Intro video sent to the owner for review** |
| 6 · Display optimisation | Presenter on 1280 × 1024: aspect, integer/pixel-perfect, rotation, bezels/backgrounds, per-game settings, safe recovery | Horizontal and vertical CPS1 games look right; a bad setting always reverts without a keyboard |
| 7 · Game library | Background scan, ROM-set verification with clear messages, metadata, artwork, favourites, recently played, playtime, collections | Large library scans with the UI staying smooth; works fully offline |
| 8 · Polish & testing | Input latency (frame delay, run-ahead), startup time, launch/exit reliability, stability, Steam Deck testing, docs, release | Cabinet checklist passed; 1.0 AppImage |

## Where things stand (0.2)

| Phase | State |
| --- | --- |
| 2 | Done on the build server; Deck check pending |
| 3 | Implemented: assignment by port/serial/name, assign-by-press, remap, swap, reset, reconnect, test card, auto-coin, exit holds. Needs the cabinet gate |
| 4 | 4b-4e implemented for sf2, sf2ce, ffight, 1941; save states and NVRAM not yet. Needs real-ROM verification |
| 5 | Intro (with sound), grid, details, favourites, recent, collections, search. Intro video sent for review |

## Phase 4 milestones (engine)

| Milestone | Result you can see |
| --- | --- |
| 4a · Framework | Scheduler, bus, device model, save states; CPU adapters pass the 68000 and Z80 test suites in CI |
| 4b · CPS1 sound board | Z80 + YM2151 + OKI MSM6295 run the *Street Fighter II* sound program: music and effects play |
| 4c · CPS1 video | Tilemaps, sprites, palettes, priorities; attract-mode screens render correctly (checked against reference frames) |
| 4d · First playable | *Street Fighter II* playable with both panels, coins/start, correct six-button mapping |
| 4e · More CPS1 | Further CPS1 titles via the game database, including a vertical game; NVRAM and save states |

## Intro animation review

When the intro is built (Phase 5), a **video recording of it is sent to the
owner** for comments before it is finalised, and again after each revision.
