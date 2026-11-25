// SPDX-License-Identifier: GPL-3.0-or-later
//
// The engine's list of supported games: every machine module adds its own.
// A game is listed only when its hardware is implemented by a Zen1 module.
#include "zen1/emu/games.hpp"
#include "zen1/emu/zip.hpp"

#include <cctype>
#include <set>

#include "c1942.hpp"
#include "consoles.hpp"
#include "cps1.hpp"
#include "galaxian.hpp"
#include "neogeo.hpp"
#include "pacman.hpp"

namespace zen1::cps1 {
std::vector<emu::GameInfo> moduleGames();
std::vector<emu::RomFile> moduleRomFiles(const std::string& set);
std::unique_ptr<emu::Machine> moduleCreate(const std::string& set, const std::filesystem::path& gamesDir,
                                           std::string* error);
}  // namespace zen1::cps1

namespace zen1::emu {

namespace {
struct Module {
    std::vector<GameInfo> (*games)();
    std::vector<RomFile> (*roms)(const std::string&);
    std::unique_ptr<Machine> (*create)(const std::string&, const std::filesystem::path&, std::string*);
    // Optional: a module whose sets need more than "every listed file" (a
    // choice of BIOS, for example) reports missing files itself.
    std::vector<std::string> (*missing)(const std::string&, const std::filesystem::path&) = nullptr;
};
const Module kModules[] = {
    {cps1::moduleGames, cps1::moduleRomFiles, cps1::moduleCreate},
    {pacman::moduleGames, pacman::moduleRomFiles, pacman::moduleCreate},
    {galaxian::moduleGames, galaxian::moduleRomFiles, galaxian::moduleCreate},
    {neogeo::moduleGames, neogeo::moduleRomFiles, neogeo::moduleCreate, neogeo::moduleMissing},
    {c1942::moduleGames, c1942::moduleRomFiles, c1942::moduleCreate},
};
}  // namespace

const std::vector<GameInfo>& supportedGames() {
    static const std::vector<GameInfo> list = [] {
        std::vector<GameInfo> v;
        for (const auto& m : kModules)
            for (auto& g : m.games()) v.push_back(std::move(g));
        return v;
    }();
    return list;
}

const GameInfo* findGame(const std::string& set) {
    for (const auto& g : supportedGames())
        if (g.set == set) return &g;
    return nullptr;
}

std::vector<RomFile> romFilesFor(const std::string& set) {
    for (const auto& m : kModules) {
        auto r = m.roms(set);
        if (!r.empty()) return r;
    }
    return {};
}

std::vector<std::string> missingRomFiles(const std::string& set, const std::filesystem::path& gamesDir) {
    for (const auto& m : kModules) {
        auto r = m.roms(set);
        if (r.empty()) continue;
        if (m.missing) return m.missing(set, gamesDir);
        const auto* g = findGame(set);
        return checkRomSet(gamesDir, set, g ? g->parent : std::string(), r);
    }
    return {set + ".zip"};
}

std::optional<std::string> identifyZip(const std::filesystem::path& zipPath, const std::filesystem::path& gamesDir) {
    ZipArchive zip;
    if (!zip.open(zipPath)) return std::nullopt;
    std::set<std::uint32_t> inZip;
    for (const auto& e : zip.entries()) inZip.insert(e.crc);
    const auto& index = crcIndex(gamesDir);
    std::string stem = zipPath.stem().string();
    for (auto& c : stem) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::optional<std::string> best;
    int bestScore = -1;
    for (const auto& m : kModules) {
        for (const auto& g : m.games()) {
            const auto files = m.roms(g.set);
            int here = 0, total = 0;
            bool complete = true;
            for (const auto& f : files) {
                if (f.load == RomLoad::Continue) continue;
                ++total;
                if (inZip.count(f.crc)) ++here;
                else if (index.find(f.crc).empty()) complete = false;
            }
            if (here == 0 || !complete) continue;
            // The program ROMs of the right revision are what tell sets apart:
            // most files of this zip should belong to the candidate.
            int score = here * 4 + (g.set == stem ? 2 : 0) + (g.parent.empty() ? 1 : 0);
            if (score > bestScore) {
                bestScore = score;
                best = g.set;
            }
        }
    }
    return best;
}

std::unique_ptr<Machine> createMachine(const std::string& set, const std::filesystem::path& gamesDir,
                                       std::string* error) {
    // A console cartridge registered by the engine (--rom FILE).
    if (consoles::registeredRom(set)) return consoles::create(set, error);
    if (!findGame(set)) {
        if (error) *error = "'" + set + "' is not supported by the Zen1 engine";
        return nullptr;
    }
    for (const auto& m : kModules) {
        if (m.roms(set).empty()) continue;
        std::string err;
        auto machine = m.create(set, gamesDir, &err);
        if (!machine && error) *error = err;
        return machine;
    }
    return nullptr;
}

}  // namespace zen1::emu
