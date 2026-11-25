// SPDX-License-Identifier: GPL-3.0-or-later
//
// Games Zen1 can recognise but cannot run yet. They appear in the library as
// NOT SUPPORTED with their real details, and say what hardware is missing.
// Set names follow MAME. See docs/HARDWARE_RESEARCH.md for the reasoning.
#include "zen1/emu/known_games.hpp"

namespace zen1::emu {

const std::vector<KnownGame>& knownGames() {
    static const std::vector<KnownGame> list = {
        // set, title, year, maker, hardware, players, joystick, buttons, bios, missing in Zen1
        {"galaga", "Galaga", "1981", "Namco", "Namco Galaga", 2, "2-way", 1, "",
         "three-Z80 board, Namco 05xx starfield and 51xx/54xx custom chips"},
        {"digdug", "Dig Dug", "1982", "Namco", "Namco Galaga", 2, "4-way", 1, "",
         "three-Z80 board, Namco 51xx/53xx custom chips"},
        {"frogger", "Frogger", "1981", "Konami", "Konami Galaxian-based", 2, "4-way", 0, "",
         "Galaxian-style video and the AY-3-8910 sound board"},
        {"dkong", "Donkey Kong", "1981", "Nintendo", "Nintendo Radar Scope", 2, "4-way", 1, "",
         "Intel 8035 sound CPU and the analogue sound circuits"},
        {"invaders", "Space Invaders", "1978", "Taito / Midway", "Midway 8080", 2, "2-way", 1, "",
         "Intel 8080 CPU and the analogue sound circuits"},
        {"1942", "1942", "1984", "Capcom", "Capcom 1942", 2, "8-way", 2, "",
         "Capcom Z80 video board and AY-3-8910 sound"},
        {"gng", "Ghosts'n Goblins", "1985", "Capcom", "Capcom Ghosts'n Goblins", 2, "8-way", 2, "",
         "Motorola 6809 CPU and YM2203 sound"},
        {"gunsmoke", "Gun.Smoke", "1985", "Capcom", "Capcom Gun.Smoke", 2, "8-way", 3, "",
         "Capcom Z80 video board and YM2203 sound"},
        {"btime", "BurgerTime", "1982", "Data East", "Data East BurgerTime", 2, "4-way", 1, "",
         "MOS 6502 CPUs and AY-3-8910 sound"},
        {"ddragon", "Double Dragon", "1987", "Technos", "Technos Double Dragon", 2, "8-way", 3, "",
         "Hitachi 6309 CPU, 63701 MCU, 6809 sound CPU, MSM5205"},
        {"contra", "Contra", "1987", "Konami", "Konami Contra", 2, "8-way", 2, "",
         "Hitachi 6309 CPU and Konami 007121 video"},
        {"nemesis", "Nemesis", "1985", "Konami", "Konami Nemesis", 2, "8-way", 3, "",
         "Konami Nemesis video and K005289 sound"},
        {"gradius", "Gradius", "1985", "Konami", "Konami Nemesis", 2, "8-way", 3, "",
         "Konami Nemesis video and K005289 sound"},
        {"rtype", "R-Type", "1987", "Irem", "Irem M72", 2, "8-way", 2, "",
         "NEC V30 CPU and Irem M72 video"},
        {"raiden", "Raiden", "1990", "Seibu Kaihatsu", "Seibu Raiden", 2, "8-way", 2, "",
         "two NEC V30 CPUs, Seibu sound system (YM3812)"},
        {"tmnt", "Teenage Mutant Ninja Turtles", "1989", "Konami", "Konami TMNT", 4, "8-way", 2, "",
         "Konami K052109/K051960 video and K007232/UPD7759 sound"},
        {"tmnt2", "Teenage Mutant Ninja Turtles: Turtles in Time", "1991", "Konami", "Konami TMNT2", 4, "8-way",
         2, "", "Konami K053245/K053251 video and K053260 sound"},
        {"ssriders", "Sunset Riders", "1991", "Konami", "Konami TMNT2", 4, "8-way", 2, "",
         "Konami K053245/K053251 video and K053260 sound"},
        {"simpsons", "The Simpsons", "1991", "Konami", "Konami Simpsons", 4, "8-way", 2, "",
         "Konami 053248 CPU, K053247 video, K053260 sound"},
        {"dino", "Cadillacs and Dinosaurs", "1993", "Capcom", "Capcom CPS-1 (QSound)", 3, "8-way", 3, "",
         "CPS-1 QSound sound board (Kabuki Z80 and DSP16A)"},
        {"punisher", "The Punisher", "1993", "Capcom", "Capcom CPS-1 (QSound)", 2, "8-way", 2, "",
         "CPS-1 QSound sound board (Kabuki Z80 and DSP16A)"},
        {"wof", "Warriors of Fate", "1992", "Capcom", "Capcom CPS-1 (QSound)", 3, "8-way", 2, "",
         "CPS-1 QSound sound board (Kabuki Z80 and DSP16A)"},
        {"sf2t", "Street Fighter II': Hyper Fighting (USA)", "1992", "Capcom", "Capcom CPS-1", 2, "8-way", 6, "",
         "this regional version; Zen1 runs the World version: add sf2hf.zip"},
        {"ssf2", "Super Street Fighter II", "1993", "Capcom", "Capcom CPS-2", 2, "8-way", 6, "",
         "CPS-2 (encrypted 68000, QSound)"},
        {"mslug", "Metal Slug", "1996", "Nazca / SNK", "SNK Neo Geo MVS", 2, "8-way", 3, "neogeo",
         "Neo Geo MVS (68000, Z80, YM2610, LSPC2 video) and the neogeo.zip BIOS"},
        {"mslug2", "Metal Slug 2", "1998", "SNK", "SNK Neo Geo MVS", 2, "8-way", 3, "neogeo",
         "Neo Geo MVS (68000, Z80, YM2610, LSPC2 video) and the neogeo.zip BIOS"},
        {"mk", "Mortal Kombat", "1992", "Midway", "Midway Y-Unit", 2, "8-way", 5, "",
         "TMS34010 graphics CPU and the Williams sound board"},
        {"mk2", "Mortal Kombat II", "1993", "Midway", "Midway T-Unit", 2, "8-way", 5, "",
         "TMS34010 graphics CPU and the DCS (ADSP-2105) sound board"},
        {"kinst", "Killer Instinct", "1994", "Rare / Nintendo", "Midway Killer Instinct", 2, "8-way", 6, "",
         "MIPS R4600 CPU, 3dfx Voodoo graphics and a hard-disk image (CHD)"},
        {"mt_srage", "Streets of Rage (Mega-Tech)", "1991", "Sega", "Sega Mega-Tech", 2, "8-way", 3, "megatech",
         "Mega Drive hardware (Streets of Rage was a console game; Mega-Tech is its arcade release)"},
        {"neogeo", "Neo Geo BIOS", "1990", "SNK", "SNK Neo Geo MVS", 0, "", 0, "",
         "BIOS file for Neo Geo games, not a game"},
    };
    return list;
}

const KnownGame* findKnownGame(const std::string& set) {
    for (const auto& g : knownGames())
        if (g.set == set) return &g;
    return nullptr;
}

}  // namespace zen1::emu
