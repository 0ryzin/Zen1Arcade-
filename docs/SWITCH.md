# Nintendo Switch 1 in Zen1: research, feasibility and roadmap

Status (version 0.5, November 2026): **Zen1 recognises Switch packages in
the library but cannot run them.** There is no Switch emulation code in
Zen1. This document is the research behind a possible future Switch 1
subsystem. Nothing here has been implemented beyond the package discovery
described in the first section, and nothing has been tested with a real
Switch game.

## 1. What Zen1 does today

- `.nsp` and `.xci` files in `~/Zen1Arcade/Games` appear in the library as
  NOT SUPPORTED, system "Nintendo Switch".
- Zen1 reads only the **unencrypted container headers**
  (`libs/emu/src/switch_package.cpp`): the NSP's PFS0 file table, and the
  "HEAD" magic of an XCI. The title ID comes from the ticket file name
  inside an NSP, or from the usual `[0100...]` tag in the file name; the
  name and version from the file name. Nothing is decrypted, no keys are
  read or needed.
- The game's own icon and name (the "control" NACP) are inside an
  encrypted NCA, so Zen1 cannot read them without console keys.

## 2. Target game: Ultra Street Fighter II: The Final Challengers

- Switch 1 game by Capcom, released 26 May 2017, built on Capcom's MT
  Framework engine. It offers the classic pixel graphics or redrawn HD
  graphics. US eShop application ID **01007330027EE000** (from Nintendo's
  own eShop page address).
- Why it is a good first target: a 2D fighting game with simple 3D
  (sprites on textured quads), modest CPU and GPU load compared to big 3D
  games, controls that are a stick and six buttons (a perfect fit for the
  cabinet), local 2-player in TV/table mode.
- Why it is still not easy: MT Framework games use the full Switch OS
  service set (file system, HID, audio renderer, NVN graphics through
  nvservices), so the first Switch game needs most of the generic
  infrastructure anyway. No compatibility report for this title in the
  current emulators was found during this research; its status in
  Ryubing/Eden must be checked by the owner on their own hardware.
- The arcade versions it is based on (*Street Fighter II* family on CPS-1,
  *Super Street Fighter II Turbo* and *Hyper Street Fighter II* on CPS-2)
  already run in Zen1's own engine (untested with real ROMs).

## 3. Projects researched

### Ryujinx and Ryubing (C#, MIT)

- Ryujinx was written from scratch in C# from 2017 and discontinued on
  1 October 2024. Ryubing is the community continuation (latest stable
  1.3.x in 2026). Licence MIT, with libvpx (BSD) and FFmpeg (LGPL-3).
- Architecture (well separated, about 30 projects):
  - **ARMeilleure**: its own JIT. ARMv8 (and ARM32) guest code is decoded,
    turned into an intermediate representation, optimised (SSA), and
    compiled to x86-64 or ARM64; a persistent profiled translation cache
    (PPTC) makes later starts fast.
  - **Ryujinx.HLE**: the Horizon OS in high-level emulation: kernel
    (processes, threads, memory, synchronisation), IPC and a service
    manager with each system service reimplemented in C#.
  - **Ryujinx.Graphics.\***: Maxwell GPU command processing, a graphics
    abstraction layer (GAL) with Vulkan/OpenGL backends, a shader
    translator, texture handling, disk shader cache.
  - **LibHac** for the file system (NCA, PFS0, XCI, save data, keys).
- Compatibility as of May 2024: about 4,300 titles tested, about 4,100
  past the menus, about 3,550 considered playable.

### Eden (C++, GPL-3.0-or-later)

- A Yuzu-derived Switch 1 emulator in C++ (public pre-alpha May 2025;
  0.2.x stable in 2026), licensed GPL-3.0-or-later, with Windows, Linux,
  macOS, Android and FreeBSD builds and Steam Deck builds.
- Architecture inherited from Yuzu: **dynarmic** (ARM to x86-64 / ARM64
  recompiler, now maintained in-house by Eden), HLE of the Horizon kernel
  and services (`core/hle`), file-system loaders for NSP/XCI/NCA,
  `video_core` with Vulkan and OpenGL renderers and a shader recompiler
  (Maxwell shaders to SPIR-V/GLSL), cubeb for audio, SDL for input.
- Stated minimum requirements: an x86-64 CPU with FMA (Intel Core
  i5-4430 / AMD Ryzen 3 1200 minimum) or ARM64-v8a, 6 threads or more
  recommended; Vulkan GPU.
- Known weak spots reported by users: shader-compilation stutter, and
  nightly builds that fix one game and break another.

### Others (not studied in depth)

Citron and Sudachi (Yuzu-derived, C++, GPL), Kenji-NX and MeloNX
(Ryujinx-derived). None is a better integration candidate than Eden.

## 4. Can any of it go into Zen1?

| Option | Licence | Technical fit | Verdict |
| --- | --- | --- | --- |
| Eden code inside Zen1 (as a `machines/switch` subsystem) | GPL-3.0-or-later into GPL-3.0-or-later: allowed, keep notices, ship source | C++ like Zen1. But it is a whole emulator (hundreds of thousands of lines, many dependencies: dynarmic, Vulkan, Boost, Qt-free core possible) | **The only realistic way to run Switch games in Zen1 within years, not decades.** It is Zen1 embedding Eden's core, built from source into Zen1, not a hidden external process |
| Ryubing / Ryujinx code | MIT: compatible with GPL-3 | C#/.NET: would mean embedding the .NET runtime in Zen1 | Possible in law, poor fit in practice |
| Parts only (e.g. dynarmic as Zen1's ARM CPU, Ryujinx's documented service behaviour as reference) | dynarmic upstream uses a permissive licence (check the exact one before use); Eden's maintained fork is GPL | A CPU alone does not run a game; the OS services and the GPU are most of the work | Useful later, not on its own |
| Zen1's own native Switch emulator | Zen1's own | ARM64 recompiler + Horizon HLE + Maxwell-to-Vulkan GPU | **Not realistic**: Ryujinx and Yuzu each took years of work by large teams |

Zen1 rule check: embedding Eden's core *source* and calling it from Zen1's
engine (Zen1 window, Zen1 input, Zen1 audio, Zen1 library) is integration,
not launching another emulator. It does mean that the Switch part of Zen1
would be mostly Eden's code, and that must be stated openly in the
licences and the documentation.

## 5. Legal, licensing and repository risks

- **Licences**: Eden (GPL-3.0-or-later) and Ryujinx (MIT) can both be
  combined with Zen1 (GPL-3.0-or-later). Zen1 would have to keep their
  copyright notices and publish the complete source of what it ships,
  which it already does.
- **Never bundled**: console keys (`prod.keys`, title keys), Switch
  firmware, and games. The owner dumps them from their own console. Zen1
  would ask for their location and never download or look them up.
- **Repository risk**: Nintendo sent coordinated takedown notices against
  Switch emulator repositories in February 2026 (13 projects named,
  including Eden, Ryubing and Citron) and August 2026 (about 400
  repositories removed), arguing that decrypting games with console keys
  circumvents copy protection, whatever the emulator's own code. Putting
  Switch emulation into the Zen1 repository would make it a likely
  target, and a takedown would also take the arcade engine offline.
  Mitigation if the owner proceeds: keep the Switch subsystem in a
  **separate repository and a separate optional download**, built as a
  plug-in module of Zen1's engine, so the arcade project stays independent.
  This is a decision for the owner.

## 6. Feasibility assessment

1. **Difficulty.** Very high. Everything Zen1 has today (six arcade
   systems in about 5,500 lines of machine-module code, about 16,000 lines
   in all, not counting the bundled CPU and sound-chip libraries) is far
   smaller than one Switch emulator. The Switch has 4 ARM Cortex-A57 cores, a Maxwell GPU driven
   through NVN command buffers and shaders, and Horizon OS with well over a
   hundred services.
2. **Partial implementation.** Not in a useful sense: a game needs the
   CPU, the kernel, the core services and the GPU before it shows one
   frame. "Partial" can only mean "few games work", not "part of each".
3. **Adapt an existing emulator legally.** Yes: Eden's core (GPL-3+) can be
   built into Zen1. It is the only path with a realistic timescale.
4. **Native implementation.** Not realistic for this project.
5. **What Zen1 lacks** for Switch: an ARM64 recompiler; a virtual memory
   manager for a 39-bit guest address space; the Horizon kernel and IPC;
   system services (fs, hid, audio renderer, nvservices, vi, am, set, time,
   acc...); NCA/NSP/XCI decryption with user keys; a Vulkan renderer (Zen1
   draws with SDL's 2D renderer); a Maxwell shader recompiler and shader
   cache; Joy-Con/Pro Controller input model.
6. **Time and complexity.**
   - Eden-core integration, first game booting inside Zen1's window with
     Zen1's input and audio: several months of full-time work, mostly on
     replacing Eden's Qt frontend glue with Zen1's engine loop, Vulkan
     presentation in Zen1's presenter, and build-system work. Keeping it in
     step with Eden's fast-moving code is ongoing work after that.
   - Native: many person-years.
7. **Hardware requirements.** As Eden's own: x86-64 with FMA (i5-4430 /
   Ryzen 3 1200 minimum), 6+ threads recommended, Vulkan 1.1+ GPU, 8 GB
   RAM or more.
8. **Steam Deck.** Yes, in principle: Zen 2 4-core/8-thread CPU with FMA,
   RDNA 2 GPU with good Vulkan drivers (RADV), 16 GB RAM. Eden and Ryubing
   publish Deck builds and many 2D and lighter 3D Switch games are
   reported to run there. Expect shader stutter on first runs and lower
   performance than on a desktop PC. Not tested by Zen1.
9. **Raspberry Pi.** Not suitable. A Pi 5's CPU is ARM (which avoids
   recompilation in theory) but its GPU and drivers are far weaker than
   the Switch's own and far from what Switch emulators need. Use the
   Steam Deck or a desktop PC.
10. **Minimum viable Switch 1 in Zen1.** Eden's core built as an optional
    Zen1 module; Zen1 lists NSP/XCI files, asks once for the user's keys
    and firmware folders, starts the game in Zen1's fullscreen window,
    maps the two arcade panels to two Pro Controllers, routes audio
    through Zen1's three-level volume, keeps the 1P/2P hold shortcuts, and
    stores saves under `~/Zen1Arcade/Saves`.
11. **USF2 as first target.** Reasonable: 2D, light, and its controls map
    one-to-one to the cabinet. Its compatibility in Eden must first be
    checked by the owner with their own dump on the Deck.
12. **Cabinet controls and local multiplayer.** Switch games see
    controllers as Joy-Cons or Pro Controllers. Each panel would be one
    Pro Controller (stick = left stick or D-pad, six buttons =
    Y X R / B A ZR or the game's own layout, 1P = +). Missing on the panels:
    right stick, ZL/ZR/L where a game needs all of them, touch screen,
    motion. Fighting, shooting and arcade-style games fit; most 3D games
    do not. Two panels give 2-player local play.

## 7. Switch identification and artwork (without keys)

- **Identification** (done): title ID from the ticket name or file name;
  title, version and kind (game, update, DLC) from the title ID and name.
- **Metadata by title ID** (proposed): the community *titledb* data
  (GitHub, JSON files mapped by title ID and eShop nsuId, with name,
  release date, number of players and Nintendo CDN `iconUrl`, `bannerUrl`
  and screenshot links). Zen1 would download only the entry for a title
  the user has, cache it in `Metadata/switch/<titleid>.json`, and fetch the
  icon/banner into `Artwork/downloaded`. Caveat: the images are Nintendo's
  and the publishers'; caching for personal display is the same use the
  console's own menu makes, but they must not be redistributed.
- **SteamGridDB** (optional): community grids, heroes and logos, searchable
  by name; needs each user's own free API key; images are user uploads.
  Useful for nicer "hero" art.
- Embedded icons stay encrypted; Zen1 does not use keys for artwork.

## 8. Proposed roadmap (Switch), only if the owner decides to go ahead

1. Owner check: Eden on the Deck in Desktop Mode, with their own keys,
   firmware and USF2 dump: does it run well? This decides everything else.
2. Zen1: title-ID metadata and artwork from titledb (no keys involved).
3. Separate repository `zen1-switch`: Eden's core as a library, built for
   Zen1's engine (`Machine` interface extended for a Vulkan picture and
   variable frame rates); first milestone: USF2 to its title screen inside
   Zen1's window.
4. Input mapping to Pro Controllers, audio through Zen1, saves in Zen1's
   folders, 1P/2P shortcuts.
5. Only then: more games, Deck tuning, shader cache handling.

The arcade engine stays the priority throughout.

## Sources

- Ryujinx / Ryubing: https://codeberg.org/Software_arch/Ryubing (licence,
  credits), architecture overview of the Ryujinx code base
  https://www.mintlify.com/yakushabb/mirror-ryujinx/architecture/overview
  and /armeilleure, /hle; compatibility figures from the Ryujinx README
  mirror https://gitea.com/eduardo78812/ryujinx-mirror-new/src/
- Eden: https://eden-emulator.github.io/ (requirements page), AUR package
  https://aur.archlinux.org/packages/eden (GPL-3.0-or-later), source
  https://git.eden-emu.dev/eden-emu/eden
- 2026 takedowns and the state of the projects: tech-insider.org articles
  on Eden, Citron and Ryubing (2026); RetroDECK's February 2026 notice
  https://retrodeck.readthedocs.io/en/latest/blog/2026/02/19/february-2026---extra-switch-emulation-support---will-be-removed/
- USF2: https://en.wikipedia.org/wiki/Ultra_Street_Fighter_II ;
  https://ec.nintendo.com/apps/01007330027EE000/US
- titledb: https://github.com/blawar/titledb ; SteamGridDB API:
  https://www.steamgriddb.com/api/v2
