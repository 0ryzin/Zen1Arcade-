// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

namespace zen1::emu {

// A game Zen1 recognises but its engine cannot run yet.
struct KnownGame {
    std::string set;
    std::string title;
    std::string year;
    std::string manufacturer;
    std::string hardware;
    int players = 2;
    std::string joystick;
    int buttons = 0;
    std::string bios;     // BIOS set the game needs, if any
    std::string missing;  // what Zen1's engine lacks for it
};

const std::vector<KnownGame>& knownGames();
const KnownGame* findKnownGame(const std::string& set);

}  // namespace zen1::emu
