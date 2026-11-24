// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal read-only ZIP archive reader (stored and deflate entries), used to
// load ROM sets without extracting them.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace zen1::emu {

struct ZipEntry {
    std::string name;       // path inside the archive
    std::uint32_t crc = 0;
    std::uint32_t compressedSize = 0;
    std::uint32_t size = 0;
    std::uint16_t method = 0;
    std::uint32_t localHeaderOffset = 0;
};

class ZipArchive {
public:
    bool open(const std::filesystem::path& file, std::string* error = nullptr);
    const std::vector<ZipEntry>& entries() const { return entries_; }
    const std::filesystem::path& path() const { return path_; }

    // Finds by CRC (preferred: robust against renamed files), else by
    // case-insensitive file name (ignoring folders inside the archive).
    const ZipEntry* find(std::uint32_t crc, const std::string& name) const;

    // Reads and inflates an entry; verifies its CRC.
    std::optional<std::vector<std::uint8_t>> read(const ZipEntry& e, std::string* error = nullptr) const;

private:
    std::filesystem::path path_;
    std::vector<ZipEntry> entries_;
};

std::uint32_t crc32(const std::uint8_t* data, size_t size);

}  // namespace zen1::emu
