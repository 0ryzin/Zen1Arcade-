# Zen1 Arcade — Research notes

Summary of what was researched in Phase 1 and what Zen1 takes from it (ideas
only; no code is copied).

## Frontends and platforms

| Project | Licence | What we learned |
| --- | --- | --- |
| [RetroArch](https://github.com/libretro/retroarch) | GPL-3.0 | Reference for features (shaders, run-ahead latency reduction, rewind). Zen1 does **not** use or launch it |
| [Attract-Mode](https://github.com/mickelson/attract) / [Attract-Mode Plus](https://github.com/oomek/attractplus) | GPL-3.0 | Cabinet-first, joystick-only frontend; hides the OS |
| [EmulationStation](https://github.com/Aloshi/EmulationStation) | MIT | System/launch definitions, per-system metadata, first-run input setup |
| [Batocera EmulationStation](https://github.com/batocera-linux/batocera-emulationstation) | MIT | Kiosk mode, metadata editor |
| [ES-DE](https://www.es-de.org/) | MIT | Actively maintained controller-driven frontend |
| [Pegasus](https://github.com/mmatyas/pegasus-frontend) | GPL-3.0 | Qt/QML frontend focused on performance |
| [Lunchpail](https://github.com/benwbooth/lunchpail) | MIT | Rust + Qt/QML library with a gamepad "Couch Mode" |
| [FliperOS](https://github.com/juniorterin/fliperOS) | none stated | CRT/15 kHz cabinet distribution; no code reusable |

Common user problems found: Player 1/2 swapping after reboot with identical
encoders; six-button layouts landing on the wrong buttons; version-locked
arcade ROM sets failing with no explanation; anything that drops the user
onto a desktop.

## Emulation components

| Component | Licence | Notes |
| --- | --- | --- |
| [Musashi](https://github.com/kstenerud/Musashi) (68000, C) | MIT-style (current version; very old copies were non-commercial) | Long used in MAME |
| [Moira](https://dirkwhoffmann.github.io/Moira/docs/Overview/About.html) (68000, C++) | MIT | Core of vAmiga; precise timing |
| [floooh chips `z80.h`](https://github.com/floooh/chips) (Z80, C) | zlib | Cycle-stepped |
| [superzazu z80](https://github.com/superzazu/z80) (Z80, C99) | MIT | Passes ZEXDOC/ZEXALL |
| [ymfm](https://github.com/aaronsgiles/ymfm) (Yamaha FM, C++) | BSD-3-Clause | YM2151, YM2610 and others |
| [SingleStepTests](https://github.com/SingleStepTests) | MIT | JSON CPU test suites (68000, Z80) |
| [libretro API header](https://docs.libretro.com/development/cores/developing-cores/) | MIT | Only relevant to the optional compatibility module |
| [MAME](https://www.mamedev.org/?p=422) | GPL-2.0+ overall, >90% of files BSD-3 | Reference for hardware behaviour; its CPS1 files are BSD-3-Clause |
| [FBNeo](https://docs.libretro.com/library/fbneo) | Non-commercial | Cannot be shipped with Zen1 |

## CPS1 hardware references

- CPS1: 68000 at 10 MHz, Z80 at 3.579 MHz, YM2151, OKI MSM6295, 384 × 224 at
  ~59.6 Hz ([arcade-history](https://www.arcade-history.com/game/69707/)).
- Fabien Sanglard's CPS-1 writing and tools
  ([ccps](https://fabiensanglard.net/ccps), [SF2 sound system](https://fabiensanglard.net/sf2_sound_system/)).

## Steam Deck

- Desktop Mode is KDE Plasma; Wayland by default since SteamOS 3.8
  ([KDE](https://planet.kde.org/marco-martin-2026-07-01-steam-deck-and-wayland/)).
- System partition is read-only; AppImages run without modifying it
  ([Steam Support](https://help.steampowered.com/faqs/view/671A-4453-E8D2-323C)).
- In Desktop Mode Steam can expose a virtual gamepad for physical controllers
  ([GNOME blog](https://blogs.gnome.org/alicem/2024/10/)).
- Controller names given in Steam are stored in Steam's own config files, not
  in Linux ([GamingOnLinux forum](https://gamingonlinux.com/forum/topic/5757)).

## Display

- *Ultra Street Fighter II: The Final Challengers* is a 2017 Nintendo Switch
  title ([Nintendo](https://www.nintendo.co.uk/News/2017/May/In-shops-and-on-Nintendo-eShop-now-ULTRA-STREET-FIGHTER-II-The-Final-Challengers-1228310.html)),
  not arcade hardware.
