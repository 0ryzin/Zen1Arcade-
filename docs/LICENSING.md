# Zen1 Arcade — Licensing

**Decision (24 Nov 2026): Zen1 Arcade is licensed under the GNU General
Public License, version 3 or (at your option) any later version
(GPL-3.0-or-later).** The full text is in [`LICENSE`](../LICENSE).

Reason: it keeps it legally possible to integrate compatible GPL-licensed
emulation code in the future if needed.

## What this means in practice

- Zen1's own source files carry the header:
  `SPDX-License-Identifier: GPL-3.0-or-later`.
- Anyone who receives a built copy of Zen1 is entitled to its source.
  Zen1 is currently for the owner's personal use; if it is ever shared, the
  source must be offered alongside it.
- Third-party code may only be added if its licence is **compatible with
  GPL-3.0**: GPL-3.0, GPL-2.0-*or-later*, LGPL, MIT, zlib, BSD-2/3,
  BSL-1.0, Apache-2.0 and public domain are compatible.
  GPL-2.0-*only* code and any "non-commercial" licence are **not**.

## Rules for every third-party component

1. Review the licence **before** integration; record name, version,
   licence, source URL and how it is used in `THIRD_PARTY.md`.
2. Vendored code keeps its original licence file and copyright notices in
   `third_party/<name>/`.
3. Attribution: the About screen and the AppImage include all third-party
   notices.
4. No code from projects with unknown or incompatible licences
   (e.g. FliperOS: no licence stated; FBNeo and MAME 2003-Plus:
   non-commercial).
5. MAME is used as a behaviour reference. If a MAME file is ever ported, it
   must carry a GPL-compatible licence (BSD-3-Clause or GPL-2.0+), keep its
   copyright header and be listed in `THIRD_PARTY.md`.

## No external emulators or cores

By decision, Zen1 never loads third-party emulator cores, never launches
other emulators and never falls back to RetroArch. Only hardware that
Zen1's own engine implements is supported; anything else is shown as
"Unsupported".

## Content

Zen1 never distributes games, ROMs or BIOS files.

*This is a practical summary, not legal advice.*
