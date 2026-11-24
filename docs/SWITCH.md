# Nintendo Switch (NSP / XCI): investigation

Status: **investigated, not planned for the arcade build.** Nothing here
changes Zen1's arcade engine.

## What the formats are

| Format | What it is | Needs |
| --- | --- | --- |
| `.xci` | Image of a game cartridge | The console's `prod.keys` to decrypt, plus system firmware for many games |
| `.nsp` | eShop package (game, update or DLC) | Same keys; updates and DLC are separate NSPs |

Keys and firmware are Nintendo's copyrighted, console-specific data. A
user can only get them from their own console (Ryubing's documentation has
"Key Dump" and "Firmware Dump" guides for that). Zen1 would never ship,
download or find them.

## The hardware

The Switch is a modern console: 4 ARM Cortex-A57 cores (NVIDIA Tegra X1),
a Maxwell-class GPU programmed through NVN/Vulkan-like command buffers, a
full operating system (Horizon) with hundreds of system services, and
Joy-Con / Pro Controller input. Emulating it means:

- dynamic recompilation of ARM64 to x86-64;
- reimplementing the Horizon OS services (high-level emulation);
- translating the GPU's shaders and commands to Vulkan, with a shader cache.

That is a different kind of project from an arcade engine. The
compatibility of the best open-source attempt took several years of
full-time team work to reach.

## Ryujinx / Ryubing

- Ryujinx was an open-source Switch emulator written in C# under the MIT
  licence. It was discontinued on 1 October 2024; Ryubing is the community
  fork that continues it (MIT, with libvpx under BSD and FFmpeg under
  LGPL-3).
- Its documentation recommends at least 8 GiB of RAM. The Steam Deck has
  16 GiB.

## Can it be part of Zen1?

- **Licence:** MIT code may legally be combined with GPL-3.0 code, with the
  MIT notice kept.
- **Technically:** Ryujinx is a large .NET (C#) application. Zen1 is C++.
  "Integrating" it would mean embedding the .NET runtime and Ryujinx itself,
  which makes it Ryujinx running inside Zen1, not Zen1's own emulator.
  That conflicts with the project rule that Zen1 does not become a frontend
  for another emulator.
- **Writing Zen1's own Switch emulator** is a multi-year effort and is
  outside the arcade engine.

**Recommendation:** keep the arcade engine the focus. If Switch support is
still wanted later, the honest options are:

1. a separate, clearly labelled Zen1 "console" add-on that the owner
   explicitly agrees may embed an MIT-licensed emulator; or
2. not supporting it.

Either way: the user supplies their own keys and firmware, and nothing
from Nintendo is ever bundled. The cabinet controls (two sticks, six
buttons each) also map poorly to most Switch games, which expect two analog
sticks, ZL/ZR and touch.
