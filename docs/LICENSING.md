# Zen1 Arcade — Licensing

**No licence has been chosen.** This file explains the choice. No `LICENSE`
file will be added until the owner decides.

## MIT vs GPL-3.0, in practical terms for Zen1

| Question | MIT | GPL-3.0 |
| --- | --- | --- |
| Can anyone use, copy, modify and redistribute Zen1? | Yes | Yes |
| Must people who distribute a modified Zen1 publish their source? | **No.** They can make a closed version | **Yes.** Any distributed modified version must be GPL with source available |
| Could someone sell a closed product built on Zen1? | Yes, keeping only the copyright notice | Only if they also release their source under GPL |
| Can Zen1 include MIT, zlib, BSD-3 code (68000/Z80 cores, ymfm, SDL3)? | Yes | Yes |
| Can Zen1 include GPL code (e.g. GPL-only parts of MAME, GPL emulator cores)? | **No** (the combination would have to be GPL) | **Yes** (GPL-2.0-*or-later* code is compatible) |
| Can Zen1 ever ship non-commercial-licensed cores (FBNeo, MAME 2003-Plus)? | No | No |
| Qt 6 (LGPL-3.0, linked dynamically) | Fine | Fine |
| Can the owner change licence later? | Easier while the owner is the only copyright holder; harder once others contribute | Same |

## What this means here

- **The planned components (Moira/Musashi, z80.h, ymfm, SDL3, Qt LGPL,
  SQLite) are compatible with both.** The choice does not block the plan.
- **GPL-3.0 keeps more doors open for emulation**: it lets us legally reuse
  GPL code (e.g. MAME files that are GPL rather than BSD-3) and host GPL
  cores in the optional compatibility module. It also guarantees that every
  distributed fork of Zen1 stays open.
- **MIT is simpler and maximally permissive**, but rules out incorporating
  any GPL emulation code, ever, unless the licence is changed.
- **Recommendation:** GPL-3.0-or-later, mainly because arcade emulation
  knowledge and code in the open-source world is largely GPL/BSD, and GPL
  keeps that path available. The decision is the owner's.

## Rules that apply whichever licence is chosen

1. Every third-party component is recorded in `THIRD_PARTY.md` with name,
   version, licence, source URL and how it is used. CI will fail on an
   unrecorded dependency.
2. Vendored code keeps its original licence file in `third_party/<name>/`.
3. No code is copied from projects whose licence is unknown or
   incompatible (e.g. FliperOS states no licence; FBNeo is non-commercial).
4. MAME source is used **as a reference** for hardware behaviour. If any
   MAME file is ever ported, only files marked `license:BSD-3-Clause` (or
   GPL, if Zen1 is GPL) may be used, with attribution.
5. Facts about hardware (memory maps, register meanings, ROM hashes) are
   documented from multiple sources and written in our own form.
6. Zen1 never distributes games, ROMs or BIOS files.

*This is a practical summary, not legal advice.*
