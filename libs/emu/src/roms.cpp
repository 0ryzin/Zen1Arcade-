// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/emu/roms.hpp"

#include "zen1/emu/zip.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fs = std::filesystem;

namespace zen1::emu {
namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string describe(const RomFile& f) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s (crc %08x)", f.name ? f.name : "?", f.crc);
    return buf;
}

struct Sources {
    std::vector<ZipArchive> zips;  // own set first, then parent
};

Sources openSources(const fs::path& gamesDir, const std::string& set, const std::string& parent) {
    Sources s;
    for (const auto& name : {set, parent}) {
        if (name.empty()) continue;
        if (auto p = findSetZip(gamesDir, name)) {
            ZipArchive z;
            if (z.open(*p)) s.zips.push_back(std::move(z));
        }
    }
    return s;
}

}  // namespace

std::optional<fs::path> findSetZip(const fs::path& gamesDir, const std::string& set) {
    const std::string want = lower(set) + ".zip";
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(gamesDir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file(ec) && lower(it->path().filename().string()) == want) return it->path();
    }
    // BIOS sets (neogeo.zip) may live in ~/Zen1Arcade/BIOS next to Games.
    const fs::path bios = gamesDir.parent_path() / "BIOS";
    if (gamesDir.filename() != "BIOS" && fs::is_directory(bios, ec)) return findSetZip(bios, set);
    return std::nullopt;
}

std::vector<std::string> checkRomSet(const fs::path& gamesDir, const std::string& set,
                                     const std::string& parent, const std::vector<RomFile>& files) {
    const auto src = openSources(gamesDir, set, parent);
    std::vector<std::string> missing;
    if (src.zips.empty()) {
        missing.push_back(set + ".zip");
        return missing;
    }
    for (const auto& f : files) {
        if (f.load == RomLoad::Continue) continue;
        bool found = false;
        for (const auto& z : src.zips) {
            const ZipEntry* e = z.find(f.crc, f.name);
            if (e && e->crc == f.crc) { found = true; break; }
        }
        if (!found) missing.push_back(describe(f));
    }
    return missing;
}

RomLoadResult loadRomSet(const fs::path& gamesDir, const std::string& set, const std::string& parent,
                         const std::vector<RomRegion>& regions, const std::vector<RomFile>& files) {
    RomLoadResult r;
    for (const auto& reg : regions) r.regions[reg.name].assign(reg.size, reg.fill);

    const auto src = openSources(gamesDir, set, parent);
    if (src.zips.empty()) {
        r.missing.push_back(set + ".zip not found in " + gamesDir.string());
        return r;
    }
    r.zipUsed = src.zips.front().path().string();

    std::vector<std::uint8_t> data;   // current file (kept for Continue)
    std::uint32_t consumed = 0;       // bytes of `data` already placed
    RomLoad mode = RomLoad::Plain;    // a Continue keeps the previous file's layout
    for (const auto& f : files) {
        if (f.load != RomLoad::Continue) mode = f.load;
        if (f.load != RomLoad::Continue) {
            data.clear();
            consumed = 0;
            bool got = false;
            for (const auto& z : src.zips) {
                const ZipEntry* e = z.find(f.crc, f.name);
                if (!e || e->crc != f.crc) continue;
                if (auto bytes = z.read(*e)) {
                    data = std::move(*bytes);
                    got = true;
                    break;
                }
            }
            if (!got) {
                r.missing.push_back(describe(f));
                continue;
            }
        }
        auto it = r.regions.find(f.region);
        if (it == r.regions.end() || data.empty()) continue;
        auto& dst = it->second;
        const std::uint32_t len = std::min<std::uint32_t>(f.length, static_cast<std::uint32_t>(data.size()) - consumed);
        const std::uint8_t* s = data.data() + consumed;

        switch (mode) {
            case RomLoad::Plain:
            case RomLoad::Continue:
                for (std::uint32_t i = 0; i < len && f.offset + i < dst.size(); ++i) dst[f.offset + i] = s[i];
                break;
            case RomLoad::Byte16:
                for (std::uint32_t i = 0; i < len && f.offset + 2 * i < dst.size(); ++i) dst[f.offset + 2 * i] = s[i];
                break;
            case RomLoad::WordSwap16:
                for (std::uint32_t i = 0; i + 1 < len + 1 && f.offset + i + 1 < dst.size(); i += 2) {
                    dst[f.offset + i] = s[i + 1];
                    dst[f.offset + i + 1] = s[i];
                }
                break;
            case RomLoad::Byte64:
                for (std::uint32_t i = 0; i < len && f.offset + 8 * i < dst.size(); ++i) dst[f.offset + 8 * i] = s[i];
                break;
            case RomLoad::Word64:
                for (std::uint32_t i = 0; i + 1 < len + 1 && f.offset + 4 * i + 1 < dst.size(); i += 2) {
                    dst[f.offset + 4 * i] = s[i];
                    dst[f.offset + 4 * i + 1] = s[i + 1];
                }
                break;
        }
        consumed += len;
    }
    r.ok = r.missing.empty();
    return r;
}

}  // namespace zen1::emu
