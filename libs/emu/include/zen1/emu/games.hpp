// SPDX-License-Identifier: GPL-3.0-or-later
//
// Games Zen1 Arcade's own engine supports. A game is listed here only when
// its hardware is implemented by a Zen1 machine module.
#pragma once

#include "zen1/emu/machine.hpp"
#include "zen1/emu/roms.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zen1::emu {

struct GameInfo {
    std::string set;          // ROM set name, e.g. "sf2"
    std::string parent;       // parent set for split ROM sets, may be empty
    std::string title;
    std::string year;
    std::string manufacturer;
    std::string system;       // hardware, e.g. "Capcom CPS1"
    int players = 2;
    int buttons = 6;
    int rotation = 0;         // degrees clockwise for an upright picture
    std::string joystick = "8-way";        // 8-way, 4-way, 2-way
    std::vector<std::string> buttonNames;  // what each gameplay button does, in panel order
    std::string format = "ZIP ROM set";
    std::string bios;                      // BIOS set needed, if any
};

const std::vector<GameInfo>& supportedGames();
const GameInfo* findGame(const std::string& set);

// Required ROM files of a game (for checking a zip without running it).
std::vector<RomFile> romFilesFor(const std::string& set);

// Which supported set a zip really holds, by the CRCs inside it (any file
// name): the complete set that most of its files belong to. Lets a zip from
// another ROM-set version or with another name run as the right revision.
std::optional<std::string> identifyZip(const std::filesystem::path& zip, const std::filesystem::path& gamesDir);

// Files still needed to run a game from gamesDir (empty when ready).
std::vector<std::string> missingRomFiles(const std::string& set, const std::filesystem::path& gamesDir);

// Creates and loads a machine. On failure returns nullptr and fills `error`
// with a readable explanation (missing files, wrong ROM version, ...).
std::unique_ptr<Machine> createMachine(const std::string& set, const std::filesystem::path& gamesDir,
                                       std::string* error);

}  // namespace zen1::emu
