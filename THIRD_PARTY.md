# Third-party components

Status: **proposed, none included yet.** Each row moves to "included" only
when the component is vendored with its licence file in `third_party/`.

| Component | Purpose | Licence | Linking | Status |
| --- | --- | --- | --- | --- |
| Qt 6 (Core, Quick, Multimedia) | Frontend UI, video previews | LGPL-3.0 | Dynamic | Proposed |
| SDL3 | Engine window, GPU, audio; input in both programs | zlib | Dynamic | Proposed |
| SQLite | Library database | Public domain | Static or dynamic | Proposed |
| toml++ (or similar) | Settings files | MIT | Header-only | Proposed |
| Moira **or** Musashi | Motorola 68000 CPU | MIT / MIT-style | Static (vendored) | Evaluate in Phase 4 |
| floooh `z80.h` **or** superzazu `z80` | Zilog Z80 CPU | zlib / MIT | Static (vendored) | Evaluate in Phase 4 |
| ymfm | YM2151 (and later YM2610) FM sound | BSD-3-Clause | Static (vendored) | Proposed |
| Catch2 | Unit tests (not shipped) | BSL-1.0 | Test only | Proposed |
| SingleStepTests data | CPU tests (not shipped) | MIT | Test data only | Proposed |

Not used: RetroArch, libretro cores, FBNeo, any MAME source file (MAME is a
behaviour reference only).
