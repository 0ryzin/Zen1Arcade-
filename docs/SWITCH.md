# Nintendo Switch 1 (NSP / XCI): investigation and status

Status (version 0.5, November 2026): **Zen1 recognises Switch packages in
the library but cannot run them.** No Switch emulation code is in Zen1.

## What Zen1 does today

- `.nsp` and `.xci` files in `~/Zen1Arcade/Games` appear in the library as
  NOT SUPPORTED, system "Nintendo Switch".
- Zen1 reads only the **unencrypted container headers**
  (`libs/emu/src/switch_package.cpp`): the NSP's PFS0 file table, and the
  "HEAD" magic of an XCI. The title ID comes from the ticket file name
  inside an NSP, or from the usual `[0100...]` tag in the file name; the
  name and version from the file name. Nothing is decrypted and no keys
  are read or needed.
- The game's own icon is inside an encrypted NCA (the control NACP), so
  Zen1 cannot extract it without the console keys, and it does not look
  them up. Online artwork lookup is not done for Switch titles yet.

## Ultra Street Fighter II: The Final Challengers

A Switch 1 game (it also runs on Switch 2 through backwards
compatibility). Its NSP would be listed by Zen1 as described above. The
arcade *Street Fighter II* family (CPS-1, and *Super Street Fighter II
Turbo* / *Hyper Street Fighter II* on CPS-2) does run in Zen1's own engine.

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

## Open-source Switch emulators in 2026 (checked November 2026)

- **Eden**: a Yuzu-derived Switch 1 emulator in C++, licensed
  GPL-3.0-or-later, with Steam Deck builds. Licence-wise it could be combined
  with Zen1 (also GPL-3.0-or-later).
- **Ryubing**: the continuation of Ryujinx (C#, MIT).
- Nintendo sent coordinated takedown notices against Switch emulator
  projects in February and August 2026 (Eden, Citron, Ryubing and others,
  and several hundred repositories), arguing that decrypting games with
  console keys circumvents copy protection, regardless of the emulator's
  own code.

What that means for Zen1: embedding Switch emulation code would make the
Zen1 repository itself a likely takedown target, which would also take the
arcade engine offline. That is a decision for the owner, not something to
do quietly. Zen1 does not do it.

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

## What a real Switch subsystem in Zen1 would need

1. NSP/XCI loading with user-supplied keys (NCA decryption, title keys).
2. An ARM64 CPU recompiler (the Deck's x86-64 CPU cannot run it directly).
3. Horizon OS services in high-level emulation (file system, applets,
   audio, HID, time, graphics services...).
4. A Maxwell GPU to Vulkan translator with a shader cache.
5. Joy-Con / Pro Controller mapping onto the two arcade panels (USF2 itself
   works with a stick and six buttons).

Items 2-4 are each larger than everything in Zen1 today. Realistic paths:
embed a GPL-compatible emulator core (Eden) as a separate Zen1 subsystem,
with the legal risk above, or leave Switch out.

**Recommendation:** keep the arcade engine the focus. If Switch support is
still wanted later, the honest options are:

1. a separate, clearly labelled Zen1 "console" add-on that the owner
   explicitly agrees may embed an MIT-licensed emulator; or
2. not supporting it.

Either way: the user supplies their own keys and firmware, and nothing
from Nintendo is ever bundled. The cabinet controls (two sticks, six
buttons each) also map poorly to most Switch games, which expect two analog
sticks, ZL/ZR and touch.
