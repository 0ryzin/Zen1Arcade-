// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/emu/roms.hpp"

#include <map>

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

std::map<std::string, fs::path>& overrides() {
    static std::map<std::string, fs::path> m;
    return m;
}

struct Sources {
    std::vector<ZipArchive> zips;  // own set first, then parent, then any other zip that has a needed file
    bool own = false, parent = false;

    // A file shared between sets (graphics, sound) may sit in any zip of the
    // folder: a sibling clone, or a set from another ROM-set version.
    const ZipEntry* find(const fs::path& gamesDir, std::uint32_t crc, const char* name, const ZipArchive** from) {
        for (const auto& z : zips) {
            const ZipEntry* e = z.find(crc, name ? name : "");
            if (e && e->crc == crc) { *from = &z; return e; }
        }
        for (const auto& p : crcIndex(gamesDir).find(crc)) {
            bool open = false;
            for (const auto& z : zips) open = open || z.path() == p;
            if (open) continue;
            ZipArchive z;
            if (!z.open(p)) continue;
            zips.push_back(std::move(z));
            const ZipEntry* e = zips.back().find(crc, "");
            if (e && e->crc == crc) { *from = &zips.back(); return e; }
        }
        return nullptr;
    }
};

Sources openSources(const fs::path& gamesDir, const std::string& set, const std::string& parent) {
    Sources s;
    for (const auto& name : {set, parent}) {
        if (name.empty()) continue;
        if (auto p = findSetZip(gamesDir, name)) {
            ZipArchive z;
            if (z.open(*p)) {
                s.zips.push_back(std::move(z));
                (name == set ? s.own : s.parent) = true;
            }
        }
    }
    return s;
}

}  // namespace

void setZipOverride(const std::string& set, const fs::path& zip) {
    if (zip.empty()) overrides().erase(lower(set));
    else overrides()[lower(set)] = zip;
}

const CrcIndex& crcIndex(const fs::path& gamesDir) {
    // Built once per folder and process (a scan or a game start); a few
    // hundred zip directories take a fraction of a second.
    static std::map<std::string, CrcIndex> cache;
    const std::string key = fs::absolute(gamesDir).lexically_normal().string();
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    CrcIndex& idx = cache[key];
    std::vector<fs::path> roots{gamesDir};
    const fs::path dir = gamesDir.has_filename() ? gamesDir : gamesDir.parent_path();
    std::error_code ec;
    if (fs::is_directory(dir.parent_path() / "BIOS", ec)) roots.push_back(dir.parent_path() / "BIOS");
    for (const auto& root : roots) {
        for (auto i = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
             i != fs::recursive_directory_iterator(); i.increment(ec)) {
            if (ec) break;
            if (i.depth() > 4) { i.disable_recursion_pending(); continue; }
            if (!i->is_regular_file(ec) || lower(i->path().extension().string()) != ".zip") continue;
            ZipArchive z;
            if (!z.open(i->path())) continue;
            for (const auto& e : z.entries()) idx.byCrc[e.crc].push_back(i->path());
        }
    }
    return idx;
}

std::vector<fs::path> CrcIndex::find(std::uint32_t crc) const {
    auto it = byCrc.find(crc);
    return it == byCrc.end() ? std::vector<fs::path>{} : it->second;
}

std::optional<fs::path> findSetZip(const fs::path& gamesDir, const std::string& set) {
    if (auto o = overrides().find(lower(set)); o != overrides().end()) return o->second;
    const std::string want = lower(set) + ".zip";
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(gamesDir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file(ec) && lower(it->path().filename().string()) == want) return it->path();
    }
    // BIOS sets (neogeo.zip) may live in ~/Zen1Arcade/BIOS next to Games.
    const fs::path dir = gamesDir.has_filename() ? gamesDir : gamesDir.parent_path();  // "Games/" too
    const fs::path bios = dir.parent_path() / "BIOS";
    if (dir.filename() != "BIOS" && fs::is_directory(bios, ec)) return findSetZip(bios, set);
    return std::nullopt;
}

std::vector<std::string> checkRomSet(const fs::path& gamesDir, const std::string& set,
                                     const std::string& parent, const std::vector<RomFile>& files) {
    auto src = openSources(gamesDir, set, parent);
    std::vector<std::string> missing;
    if (!src.own) {
        missing.push_back(set + ".zip");
        return missing;
    }
    for (const auto& f : files) {
        if (f.load == RomLoad::Continue) continue;
        const ZipArchive* from = nullptr;
        if (!src.find(gamesDir, f.crc, f.name, &from)) missing.push_back(describe(f));
    }
    if (!missing.empty() && !parent.empty() && !src.parent)
        missing.push_back("(these may be in the parent set " + parent + ".zip, which is not in the Games folder)");
    return missing;
}

RomLoadResult loadRomSet(const fs::path& gamesDir, const std::string& set, const std::string& parent,
                         const std::vector<RomRegion>& regions, const std::vector<RomFile>& files) {
    RomLoadResult r;
    for (const auto& reg : regions) r.regions[reg.name].assign(reg.size, reg.fill);

    auto src = openSources(gamesDir, set, parent);
    if (!src.own) {
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
            const ZipArchive* from = nullptr;
            if (const ZipEntry* e = src.find(gamesDir, f.crc, f.name, &from)) {
                if (auto bytes = from->read(*e)) {
                    data = std::move(*bytes);
                    got = true;
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
    if (!r.missing.empty() && !parent.empty() && !src.parent)
        r.missing.push_back("(these may be in the parent set " + parent + ".zip, which is not in the Games folder)");
    r.ok = r.missing.empty() || (r.missing.size() == 1 && r.missing[0].rfind("(these", 0) == 0);
    return r;
}

}  // namespace zen1::emu
