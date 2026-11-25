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
    Byte64,      // single bytes every 8 bytes (CPS1 graphics on 8-bit ROMs)
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

// Every ROM file (by CRC) in every zip of a folder (and its BIOS folder):
// lets a set take files it shares with other sets from wherever they are.
struct CrcIndex {
    std::map<std::uint32_t, std::vector<std::filesystem::path>> byCrc;
    std::vector<std::filesystem::path> find(std::uint32_t crc) const;
};
const CrcIndex& crcIndex(const std::filesystem::path& gamesDir);

// Use this zip for a set (a zip with another name that holds that set).
void setZipOverride(const std::string& set, const std::filesystem::path& zip);

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
