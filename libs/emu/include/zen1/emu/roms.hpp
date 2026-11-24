// SPDX-License-Identifier: GPL-3.0-or-later
//
// ROM set description and loading from zip files in the Games folder.
// Files are matched by CRC first, then by name, in the game's own zip and
// then in its parent's zip (MAME-style split sets).
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace zen1::emu {

enum class RomLoad {
    Plain,       // file copied as-is at offset
    Continue,    // next chunk of the previous file (MAME ROM_CONTINUE)
    Byte16,      // every other byte (68000 even/odd byte ROMs)
    WordSwap16,  // 16-bit words with bytes swapped
    Word64,      // 16-bit words every 8 bytes (CPS1 graphics)
};

struct RomFile {
    const char* region;
    const char* name;      // nullptr for Continue
    std::uint32_t crc;
    std::uint32_t offset;  // destination in region
    std::uint32_t length;  // bytes taken from the file
    RomLoad load = RomLoad::Plain;
};

struct RomRegion {
    const char* name;
    std::uint32_t size;
    std::uint8_t fill = 0;
};

using RegionMap = std::map<std::string, std::vector<std::uint8_t>>;

struct RomLoadResult {
    bool ok = false;
    RegionMap regions;
    std::vector<std::string> missing;  // "name (crc)" for each missing/bad file
    std::string zipUsed;
};

// Finds <set>.zip anywhere under gamesDir (case-insensitive).
std::optional<std::filesystem::path> findSetZip(const std::filesystem::path& gamesDir,
                                                const std::string& set);

RomLoadResult loadRomSet(const std::filesystem::path& gamesDir, const std::string& set,
                         const std::string& parent, const std::vector<RomRegion>& regions,
                         const std::vector<RomFile>& files);

// Only checks presence and CRCs from the zip directories (fast, no inflate).
std::vector<std::string> checkRomSet(const std::filesystem::path& gamesDir, const std::string& set,
                                     const std::string& parent, const std::vector<RomFile>& files);

}  // namespace zen1::emu
