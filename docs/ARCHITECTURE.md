# Zen1 Arcade — Architecture

Status: **proposed, awaiting approval.** Nothing here is implemented yet.

## 1. Direction in one paragraph

Zen1 Arcade is built as **two programs from one codebase**: the Zen1 frontend
(library, UI, settings, controls) and the Zen1 engine (our emulator runtime,
which hosts *machine modules* for specific arcade hardware). Both are our
code. The engine never shows a menu of its own; the frontend owns the whole
experience. The first machine module is **Capcom CPS1**. Mature, permissively
licensed CPU and sound-chip emulators are used as *components inside* our
machine modules, the way a car maker buys bearings: the scheduler, memory
maps, video hardware, game database, presentation and input are ours.

## 2. What is realistic

Writing an arcade emulator is a large, well-understood task. Writing *every*
part of one from nothing (CPU cores included) multiplies the time and the bug
count for no visible benefit. The honest split:

| Part | Who writes it | Why |
| --- | --- | --- |
| Engine framework: scheduler, bus/memory maps, device model, save states, ROM loading and verification | **Zen1 (ours)** | This is the "engine". It defines how every machine module works |
| Machine modules (CPS1 first): memory map, custom video chips (CPS-A/CPS-B), tilemaps, sprites, palettes, priorities, I/O, per-game configuration | **Zen1 (ours)** | This is where arcade emulation actually lives |
| Simple sound chips (OKI MSM6295 ADPCM) | **Zen1 (ours)** | Small, well documented |
| CPU cores (68000, Z80) | **Reused component**, behind our CPU interface | Proven cores exist under MIT/zlib licences. A self-written 68000 alone would take months to reach the same correctness |
| Complex FM sound (Yamaha YM2151, later YM2610) | **Reused component** (ymfm, BSD-3) | Accurate FM synthesis is specialist work |
| Presentation: GPU scaling, aspect, rotation, bezels, CRT effects | **Zen1 (ours)** | Central to the cabinet experience |
| Audio output, resampling, sync to display | **Zen1 (ours)** on top of SDL3 | |
| Input: device identification, Player 1/2 assignment, mapping to machine ports | **Zen1 (ours)** on top of SDL3 / Linux evdev | |
| Frontend UI, library, artwork, settings | **Zen1 (ours)** on Qt Quick | |

Every reused component sits behind a Zen1 interface (e.g. `zen1::CpuCore`), so
any of them can later be replaced by our own implementation without touching
machine modules.

**Time expectation (honest):** the engine framework plus a CPS1 module that
plays *Street Fighter II* correctly with sound is **months of focused work,
not weeks**. Phase 4 is split into small, testable milestones so progress is
visible and verifiable along the way.

## 3. Why CPS1 is the first target

| Reason | Detail |
| --- | --- |
| It runs the games that matter most to this cabinet | *Street Fighter II* (World Warrior, Champion Edition, Hyper Fighting) and many other Capcom titles run on CPS1 |
| It was designed around the six-button layout | CPS1 inputs: 8-way stick, up to six buttons, start and coin per player |
| Its picture fits the cabinet well | 384 × 224, horizontal, displayed at 4:3, ~59.63 Hz. On a 1280 × 1024 screen that is 1280 × 960 with only 32-pixel bands top and bottom |
| It includes vertical games | e.g. *1941* and *Varth*, which exercise rotation and bezel presentation early |
| It is well documented | Hardware write-ups (Fabien Sanglard's CPS-1 book and articles), chip datasheets, and MAME's BSD-3-licensed CPS1 driver as a reference |
| Its parts are available under compatible licences | 68000 and Z80 cores (MIT/zlib), YM2151 (ymfm, BSD-3), OKI MSM6295 (ours) |
| Most boards have no program encryption | Unlike CPS2, which adds encryption and QSound, so CPS2 is a natural *second* step that reuses most of the CPS1 work |

Hardware summary (CPS1): Motorola 68000 at 10 MHz (12 MHz on later boards),
Zilog Z80 at 3.579 MHz for sound, Yamaha YM2151 FM, OKI MSM6295 ADPCM,
custom CPS-A/CPS-B graphics chips.

### Later machine modules (not committed, in likely order)

1. **CPS2**: same 68000 core and most of the video; adds decryption and QSound.
2. **Neo Geo MVS**: 68000 + Z80 + YM2610 (ymfm supports YM2610).
3. **Sega System 16**: 68000 + Z80 + YM2151, reuses the same components.

Each is a new module; none requires changing the frontend or the engine
framework.

## 4. Process model

```
┌──────────────────────────────┐        ┌───────────────────────────────────┐
│ zen1-arcade (frontend)       │ launch │ zen1-engine (runner)              │
│ Qt 6 Quick UI                │───────▶│ SDL3 window, GPU presenter, audio │
│ library · settings · intro   │◀───────│ machine module (e.g. CPS1)        │
│ input service (P1/P2)        │ status │ CPU/sound components              │
└──────────────────────────────┘  IPC   └───────────────────────────────────┘
            │                                         │
            └────────────── libzen1-common ───────────┘
      (settings, data root, device identity, input profile, display plan)
```

- The frontend starts `zen1-engine` with a **launch profile** (our own JSON
  format: game, machine module, ROM set, input profile, display plan).
- They talk over a local Unix socket: `pause`, `resume`, `exit`, `status`,
  `save-state`, `load-state`, `error`.
- **Why two processes:** an emulator under development *will* crash sometimes.
  A crash in the engine must never take down the frontend or lose the user's
  place; the frontend detects it, shows a message and returns to the same tile.
  It also lets us run the engine headless in automated tests.
- **Transitions:** the frontend fades to black, the engine window opens black
  and fades in; the reverse on exit. Both are fullscreen on the same output,
  so no desktop is ever visible.

## 5. Engine design (`libzen1emu`)

| Component | Responsibility |
| --- | --- |
| **Scheduler** | Runs several CPUs at their own clock rates in time slices, keeps them synchronised, delivers interrupts and timers at exact cycle times |
| **Bus / memory map** | Per-machine address decoding: ROM, RAM, banked regions, memory-mapped I/O handlers |
| **Device model** | Every chip is a device with `reset`, `run`, `read/write`, `save/load state`. CPUs, sound chips, video chips, latches, timers, input ports, DIP switches, NVRAM |
| **Machine module** | Wires devices together for one hardware platform (e.g. `machines/cps1`) |
| **Game database** | Our own data files per game: ROM files and hashes, ROM layout, CPS-B configuration, inputs, DIP switches, orientation, native resolution and refresh |
| **ROM loader** | Reads zip sets, verifies CRC/SHA-1, merges parent/clone/BIOS, reports exactly what is missing or wrong |
| **Video output** | Machine renders a native-resolution frame (e.g. 384 × 224); the **presenter** scales it on the GPU (aspect, integer scaling, rotation, bezel/background, optional CRT effects) |
| **Audio output** | Chips generate samples at their native rates; we mix, resample and feed SDL3 with dynamic rate control so 59.63 Hz games stay smooth on a 60 Hz screen |
| **Input ports** | Map Zen1's logical cabinet layout (P1/P2 stick, six buttons, player button) onto each machine's input ports, including coin handling |
| **Save states / NVRAM** | Every device serialises its state; high scores and settings persist in `Saves/` |
| **Latency features (later)** | Frame-delay and run-ahead, once correctness is proven |

### CPU and sound components (proposed)

| Component | Use | Licence | Status |
| --- | --- | --- | --- |
| Moira (C++) or Musashi (C) | Motorola 68000 | MIT / MIT-style | Pick one in Phase 4 after testing both against the SingleStepTests 68000 suite |
| floooh `z80.h` (C) or superzazu `z80` (C) | Zilog Z80 | zlib / MIT | Pick one in Phase 4 after ZEXDOC/ZEXALL and JSON tests |
| ymfm (C++) | YM2151 now, YM2610 later | BSD-3-Clause | Proposed |
| OKI MSM6295 | ADPCM samples | Ours | To write |

## 6. Optional compatibility module (decision needed, not planned for v1)

For hardware our engine does not support yet, Zen1 *could* later host
existing emulator cores **inside the Zen1 engine process** through the
MIT-licensed libretro API header. Zen1 would own the window, presenter,
audio, input and UI; no RetroArch code or UI is involved. Every game run this
way would be labelled as such in its details screen, so nothing is disguised.

Licence consequences: GPL cores (e.g. current MAME) loaded into our process
make the distributed combination GPL; non-commercial cores (FBNeo, MAME
2003-Plus) cannot be shipped with Zen1 at all. This module is **off the plan**
unless the owner decides otherwise.

## 7. Frontend design

- **Qt 6 Quick (QML)** for the UI: GPU-rendered, animated, good text and video
  support, proven for frontends (Pegasus, Lunchpail).
- **SDL3** for controller input in both programs (Qt 6 has no maintained
  gamepad module).
- **SQLite** for the library, playtime, favourites and collections.
- **TOML** for human-editable settings.
- Screens: intro → game grid → game details → settings (controls, video,
  audio, library, application) → attract mode when idle.

## 8. Technology choices

| Choice | Reason |
| --- | --- |
| C++20 throughout | The best CPU/sound components are C/C++; one language across engine and frontend; mature Linux tooling |
| SDL3 (zlib) for the engine window, GPU, audio and input | Low-level control of frame pacing and audio latency; stable API since SDL 3.2 |
| Qt 6 Quick (LGPL-3.0, dynamically linked) for the frontend | Animation, transitions and video previews without writing a UI toolkit |
| CMake + Ninja, GCC/Clang | Standard; builds in a SteamOS-compatible container |
| AppImage packaging | Runs on SteamOS's read-only system without modifying it; updating replaces one file |
| Catch2 unit tests, Qt Quick Test, CPU test suites in CI | Correctness evidence for every component |

Rust was considered. It would be a good engine language, but the components
we want to reuse are C/C++, and a binding layer would sit exactly where we
need the tightest control.

## 9. Steam Deck specifics

- SteamOS Desktop Mode uses KDE Plasma, Wayland by default since SteamOS 3.8.
  Under Wayland, applications do not change the display mode; Zen1 renders at
  the output's native mode (1280 × 1024) and does all scaling on the GPU. This
  also satisfies "no unnecessary resolution changes".
- When docked, the Deck's own panel may still be active as a second screen.
  Zen1 lets you pick the cabinet output once and remembers it by connector
  and monitor identity.
- In Desktop Mode, Steam can present controllers through a virtual gamepad.
  Zen1 reads the real devices and ignores Steam's virtual pads so nothing is
  doubled. Phase 2 verifies this on the real Deck.
- Zen1 is installed as an AppImage (e.g. in `~/Applications/`) and started
  from the Desktop Mode menu or a desktop shortcut. It does not install boot
  or session changes.

## 10. Module map

```
zen1arcade/
  apps/frontend/        Qt Quick UI (QML + C++ models)
  apps/engine/          zen1-engine runner (SDL3)
  libs/common/          data root, settings, device identity, IPC, display plan
  libs/input/           evdev/SDL3 device discovery, P1/P2 assignment, mapping
  libs/emu/             engine framework: scheduler, bus, devices, states
  libs/emu/cpu/         CpuCore interface + adapters for 68000 and Z80 cores
  libs/emu/sound/       mixer, resampler, OKI MSM6295, ymfm adapter
  libs/present/         GPU presenter: scaling, rotation, bezels, shaders
  machines/cps1/        CPS1 machine module + game database
  third_party/          vendored components, each with its LICENSE
  tests/                unit, CPU-suite, golden-frame and audio tests
  docs/
```
