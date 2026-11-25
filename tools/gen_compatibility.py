#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes docs/COMPATIBILITY.md from `zen1-engine --list-games`.

Usage: tools/gen_compatibility.py path/to/zen1-engine > docs/COMPATIBILITY.md

The "Tested" column comes from TESTED below and must only be changed after
someone has played that game on Zen1 with a real ROM set.
"""
import json
import subprocess
import sys

# set -> (where it was tested, notes). Empty: no real ROM set has been run.
TESTED = {}

KNOWN = {
    "Namco Galaxian": "Sound is Zen1's own approximation of the discrete sound board.",
    "SNK Neo Geo MVS": "Needs neogeo.zip (any MVS BIOS listed in docs/HARDWARE_RESEARCH.md). "
                       "Calendar chip simplified; no memory card. YM2610 runs at low fidelity "
                       "(native ~55.6 kHz) for speed.",
    "Namco Pac-Man": "",
    "Capcom CPS-1": "Clone revisions (regional versions and boards MAME runs on the same hardware) are listed; "
                    "a zip of any of them is identified by its contents.",
    "Capcom CPS-2": "Needs the set's own .key file (in current MAME sets). QSound uses Zen1's own tables unless "
                    "qsound.zip (dl-1425.bin) is in Games or BIOS: without it the sound has no 3D effect. "
                    "Three- and four-player games run with two players. Network (SSF2T link) not emulated.",
    "Capcom 1942": "AY-3-8910 sound through ymfm with Zen1's own speaker filter (MAME uses a circuit model).",
}

# Consoles: one row per system. Levels, in order: implemented (the core is
# built into Zen1), recognised (files found and named), loads (the core
# accepts a cartridge and runs it), picture (a program's screen comes out
# right), sound, input (a program reacts to the panel), playable (a game
# played through) and tested on hardware. Evidence: tools/console_smoke.sh
# (screenshots and sound levels in every CI run's console-tests.zip) and
# tests/test_consoles.cpp. Only change a level with evidence.
CONSOLES = [
    # system, files, core, picture, sound, input, playable, notes
    ("Nintendo NES / Famicom", ".nes (iNES), .zip", "QuickNES (Nes_Emu)",
     "Yes: blargg instr_test-v5 shows 'All 16 tests passed'; PeterLemon Hello World",
     "Yes: PeterLemon Twinkle (3 channels)", "Yes: blargg read_joy3 test_buttons accepts A then B",
     "Not verified", "Mappers: the ~70 QuickNES supports (NROM, MMC1-5, VRC2-7, FME-7, Namco 163...). No FDS."),
    ("Super Nintendo / Super Famicom", ".sfc, .smc, .zip", "LakeSnes",
     "Yes: PeterLemon CPU test ADC all PASS; Hello World; Mode 7 demo",
     "Yes: PeterLemon SPC700 ItaloTest", "Yes: PeterLemon ControllerLatency reacts to presses",
     "Not verified", "No enhancement chips (Super FX, SA-1, DSP-n, S-DD1, SPC7110): those games do not run. "
                     "Hi-res (512-pixel) modes are averaged to 256 pixels. Select is not on the panel."),
    ("Nintendo Game Boy", ".gb, .zip", "Gearboy",
     "Yes: blargg cpu_instrs 'Passed all tests'; PeterLemon Hello World", "Yes: PeterLemon Twinkle (4 channels)",
     "Not verified (same panel code as NES)", "Not verified", "Super Game Boy borders off."),
    ("Nintendo Game Boy Color", ".gbc, .cgb, .gb with the colour flag, .zip", "Gearboy",
     "Yes: Zen1's own colour test cartridge (CGB palette RAM, 4 colours)", "Not verified with a colour game",
     "Not verified", "Not verified", ""),
    ("Sega Master System", ".sms, .zip", "Gearsystem",
     "Yes: PeterLemon Hello World (sprites, flips)", "Yes: PeterLemon SMSAudio (PSG tone)",
     "Yes: SMSAudio menu enabled by the stick, tone held by button 1", "Not verified",
     "FM (YM2413) supported by the core, not yet heard in a test."),
    ("Sega Game Gear", ".gg, .zip", "Gearsystem",
     "Yes: Zen1's own colour test cartridge (12-bit GG palette)", "Not verified", "Not verified", "Not verified", ""),
    ("Sega SG-1000", ".sg, .zip", "Gearsystem", "Not verified (no free test cartridge found)",
     "Not verified", "Not verified", "Not verified", ""),
    ("Sega Mega Drive / Genesis", ".md, .gen, .bin (in a Mega Drive/Genesis folder), .zip", "clownmdemu",
     "Yes: SGDK samples (Hello World, 3D cube, platformer, XGM player)",
     "Yes: SGDK XGM player music (YM2612 FM)", "Yes: SGDK platformer character moves and jumps with the panel",
     "Not verified", "Six-button pad: X Y Z / A B C. Mode not on the panel. No SRAM saves yet. No Mega CD, no 32X. "
                     "Interleaved .smd dumps are not read."),
    ("PC Engine / TurboGrafx-16", ".pce (HuCards; 512-byte copier headers removed), .zip", "Geargrafx",
     "Yes: dshadoff PCE_Mouse_Test screen", "Yes: PeterLemon PlaySample",
     "Yes: PCE_Mouse_Test shows II, Select and Right pressed (joy 0 = 0026)", "Not verified",
     "CD-ROM games need a System Card BIOS, which Zen1 does not include: not offered. SuperGrafx (.sgx) not offered. "
     "PeterLemon's VDC demos rely on the VDC timing registers being set beforehand and show a rolled picture."),
]
DETECTED = [
    ("Nintendo Game Boy Advance", ".gba", "Recognised and explained only. Candidate core: mGBA (MPL-2.0, GPL-compatible)."),
    ("Nintendo 64", ".n64, .z64, .v64", "Recognised and explained only. Needs a 3D renderer and a much larger core "
                                        "(mupen64plus family): out of scope for now."),
    ("Atari 2600", ".a26", "Recognised and explained only. Candidate core: Stella (GPL-2.0-or-later)."),
    ("Atari 7800", ".a78", "Recognised and explained only. Candidate core: ProSystem (GPL-2.0)."),
    ("Sega 32X", ".32x", "Recognised and explained only: clownmdemu does not emulate the 32X."),
]


def consoles_section():
    print("## Home consoles and handhelds\n")
    print("Each system is a Zen1 machine module around an open-source core compiled into Zen1")
    print("(see THIRD_PARTY.md); Zen1 never starts another emulator. Cartridge files are found")
    print("anywhere in `Games` by their extension, also inside a `.zip`. Every listed system is")
    print("**implemented**, its files are **recognised** and its core **loads** cartridges")
    print("(tests/test_consoles.cpp runs each core). The columns after that are only Yes with")
    print("evidence from freely available test or homebrew cartridges (tools/console_smoke.sh,")
    print("run in CI; screenshots in each build's `console-tests.zip`). No commercial game has")
    print("been played on Zen1 yet, and nothing has been tried on the Steam Deck or the cabinet.\n")
    print("| System | Files | Core | Picture | Sound | Panel input | Playable game | Notes |")
    print("|---|---|---|---|---|---|---|---|")
    for row in CONSOLES:
        print("| " + " | ".join(row) + " |")
    print()
    print("Panel layouts (top row LP MP HP / bottom row LK MK HK; the player button is Start):\n")
    print("| System | Top row | Bottom row |")
    print("|---|---|---|")
    for sys_, top, bottom in (("NES, Game Boy, Game Boy Color", "B, A, Select", "B, A, Select"),
                              ("Master System, Game Gear, SG-1000", "1, 2, -", "1, 2, -"),
                              ("PC Engine", "II, I, Select", "II, I, Select"),
                              ("SNES", "Y, X, L", "B, A, R"),
                              ("Mega Drive", "X, Y, Z", "A, B, C")):
        print(f"| {sys_} | {top} | {bottom} |")
    print()
    print("Recognised, not playable (the library shows them with the reason):\n")
    print("| System | Files | Status |")
    print("|---|---|---|")
    for row in DETECTED:
        print("| " + " | ".join(row) + " |")
    print()


def main():
    out = subprocess.run([sys.argv[1], "--list-games"], check=True, capture_output=True, text=True).stdout
    data = json.loads(out)
    games = data["games"] if isinstance(data, dict) else data
    print("# Zen1 Arcade compatibility list\n")
    print("Generated by `tools/gen_compatibility.py` from the engine's own game list.")
    print("**Supported** means the hardware is implemented in Zen1 and the ROM set is")
    print("recognised. It does not mean the game has been played: the **Tested** column")
    print("says No until someone has run that game on Zen1 with a real ROM set.")
    print("The Zen1 project does not distribute ROMs or BIOS files.\n")
    print("Arcade sets are also identified by their contents: a zip whose files belong to")
    print("another revision of a game (another ROM-set version, or a renamed zip) runs as")
    print("that revision, and files shared with a parent set are found in any zip in")
    print("`Games` or `BIOS`.\n")
    systems = {}
    for g in games:
        systems.setdefault(g["system"], []).append(g)
    print("| Hardware | Games | Tested on real ROMs |")
    print("|---|---|---|")
    for s, gs in systems.items():
        t = sum(1 for g in gs if g["set"] in TESTED)
        print(f"| {s} | {len(gs)} | {t} |")
    print()
    for s, gs in systems.items():
        print(f"## {s}\n")
        if KNOWN.get(s):
            print(f"Known issues for all games on this hardware: {KNOWN[s]}\n")
        print("| Title | Set | ROM format | Players | Joystick | Buttons | Supported | Tested | Known issues |")
        print("|---|---|---|---|---|---|---|---|---|")
        for g in sorted(gs, key=lambda g: g["title"].lower()):
            fmt = g["format"]
            if g["parent"]:
                fmt += f" (needs {g['parent']}.zip)"
            buttons = ", ".join(g["buttonNames"]) or str(g["buttons"])
            tested, notes = TESTED.get(g["set"], ("No", ""))
            title = g["title"].replace("|", "/")
            print(f"| {title} ({g['year']}) | {g['set']} | {fmt} | {g['players']} | {g['joystick']} | "
                  f"{buttons} | Yes | {tested} | {notes} |")
        print()
    consoles_section()

main()
