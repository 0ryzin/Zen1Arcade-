// SPDX-License-Identifier: GPL-3.0-or-later
//
// Nintendo Switch package discovery (NSP / XCI) for the library: reads only
// the unencrypted container headers (PFS0 / HFS0 file tables) to identify a
// package. Nothing is decrypted and no keys are used: the game itself,
// including its icon (inside an encrypted NCA), stays out of reach. Zen1
// cannot run Switch games; see docs/SWITCH.md.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace zen1::emu {

struct SwitchPackage {
    std::string format;               // "NSP" or "XCI"
    std::string titleId;              // 16 hex digits, "" if unknown
    std::string kind;                 // "Game", "Update", "DLC" or ""
    std::string version;              // from the file name ("v65536"), if present
    std::string name;                 // from the file name, tags removed
    std::vector<std::string> files;   // PFS0 entries (NSP)
    bool hasTicket = false;           // a .tik inside: titlekey-encrypted eShop content
};

// PFS0 file table from raw bytes (exposed for tests).
std::optional<std::vector<std::string>> parsePfs0(const std::vector<std::uint8_t>& head);

// Reads a package's headers. nullopt if the file is not a valid NSP/XCI.
std::optional<SwitchPackage> readSwitchPackage(const std::filesystem::path& file);

// Name and IDs from a typical file name: "Title Name [0100...][v0].nsp".
void parseSwitchFileName(const std::string& stem, SwitchPackage& p);

}  // namespace zen1::emu
