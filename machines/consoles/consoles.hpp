// SPDX-License-Identifier: GPL-3.0-or-later
//
// Home consoles and handhelds. Each system is a Zen1 machine module built
// around an open-source emulation core compiled into Zen1 (see
// THIRD_PARTY.md); Zen1 never launches another emulator. A console game is
// one cartridge file (or a .zip holding one), found anywhere in the Games
// folder by its extension.
#pragma once

#include "zen1/emu/machine.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zen1::consoles {

struct System {
    std::string id;            // "nes", "snes", "gb", ...
    std::string name;          // shown in the library, e.g. "Nintendo NES / Famicom"
    std::string manufacturer;
    std::vector<std::string> extensions;  // lower case, with the dot
    int players = 2;
    std::vector<std::string> buttonNames;  // panel order: LP MP HP LK MK HK ("" = unused)
    std::string format;        // e.g. "NES cartridge (.nes)"
    std::string core;          // emulation core credited in the info panel
    bool implemented = true;   // false: detected and explained, not playable
    std::string reason;        // why not, when not implemented
};

const std::vector<System>& systems();
const System* systemById(const std::string& id);
// By file extension; `folderHint` (the folder names above the file) settles
// ambiguous extensions such as .bin (Mega Drive) .
const System* systemForFile(const std::filesystem::path& file);

struct RomImage {
    std::vector<std::uint8_t> data;
    std::string fileName;      // the cartridge file (inside a zip if it came from one)
};

// The cartridge in `file`: the file itself, or the console ROM inside a .zip.
// `system` receives the system it belongs to.
std::optional<RomImage> readRom(const std::filesystem::path& file, const System** system, std::string* error);
// For a .zip: the console system of the cartridge inside, if it holds one.
const System* systemInZip(const std::filesystem::path& zip);

// The library's set id for a cartridge file: "<system>-<file name>" made
// safe for file names (used for saves and per-game settings).
std::string setIdFor(const System& s, const std::filesystem::path& file);
// A readable title from a No-Intro / GoodTools style file name.
std::string titleFromFileName(const std::string& stem);

// Registers the file behind a console set id (the engine's --rom option).
void registerRom(const std::string& set, const std::filesystem::path& file);
std::optional<std::filesystem::path> registeredRom(const std::string& set);

std::unique_ptr<emu::Machine> create(const std::string& set, std::string* error);

}  // namespace zen1::consoles
