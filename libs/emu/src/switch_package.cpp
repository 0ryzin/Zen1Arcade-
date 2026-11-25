// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/emu/switch_package.hpp"

#include <cstddef>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <regex>

namespace zen1::emu {

namespace {
std::uint32_t le32(const std::vector<std::uint8_t>& b, size_t o) {
    return static_cast<std::uint32_t>(b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (static_cast<std::uint32_t>(b[o + 3]) << 24));
}
std::string upperHex(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string kindOf(const std::string& tid) {
    if (tid.size() != 16) return "";
    const std::string tail = tid.substr(13);
    if (tail == "800") return "Update";
    if (tail == "000") return "Game";
    return "DLC";
}
}  // namespace

std::optional<std::vector<std::string>> parsePfs0(const std::vector<std::uint8_t>& b) {
    if (b.size() < 16 || std::memcmp(b.data(), "PFS0", 4) != 0) return std::nullopt;
    const std::uint32_t n = le32(b, 4), strSize = le32(b, 8);
    if (n == 0 || n > 4096) return std::nullopt;
    const size_t table = 16, strings = table + static_cast<size_t>(n) * 24;
    if (strings + strSize > b.size()) return std::nullopt;
    std::vector<std::string> files;
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t nameOff = le32(b, table + i * 24 + 16);
        if (nameOff >= strSize) return std::nullopt;
        const char* s = reinterpret_cast<const char*>(b.data() + strings + nameOff);
        files.emplace_back(s, strnlen(s, strSize - nameOff));
    }
    return files;
}

void parseSwitchFileName(const std::string& stem, SwitchPackage& p) {
    static const std::regex tid(R"(\[([0-9A-Fa-f]{16})\])");
    static const std::regex ver(R"(\[(v\d+)\])");
    std::smatch m;
    if (p.titleId.empty() && std::regex_search(stem, m, tid)) p.titleId = upperHex(m[1]);
    if (std::regex_search(stem, m, ver)) p.version = m[1];
    std::string name = std::regex_replace(stem, std::regex(R"(\s*[\[\(][^\]\)]*[\]\)])"), "");
    std::replace(name.begin(), name.end(), '_', ' ');
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
    p.name = name.empty() ? stem : name;
    if (p.kind.empty()) p.kind = kindOf(p.titleId);
}

std::optional<SwitchPackage> readSwitchPackage(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    std::vector<std::uint8_t> head(0x10000);
    in.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
    head.resize(static_cast<size_t>(in.gcount()));
    SwitchPackage p;
    std::string ext = file.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (auto files = parsePfs0(head)) {
        p.format = "NSP";
        p.files = *files;
        for (const auto& f : p.files) {
            // Tickets are named after the rights ID: title ID + key generation.
            if (f.size() == 36 && f.substr(32) == ".tik") {
                p.hasTicket = true;
                p.titleId = upperHex(f.substr(0, 16));
            }
        }
    } else if (head.size() >= 0x104 && std::memcmp(head.data() + 0x100, "HEAD", 4) == 0) {
        p.format = "XCI";  // the title ID is inside encrypted data
    } else if (ext == ".nsp" || ext == ".xci") {
        return std::nullopt;  // the right name but not a package
    } else {
        return std::nullopt;
    }
    parseSwitchFileName(file.stem().string(), p);
    return p;
}

}  // namespace zen1::emu
