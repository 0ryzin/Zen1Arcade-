// SPDX-License-Identifier: GPL-3.0-or-later
//
// The engine's list of supported games: every machine module adds its own.
// A game is listed only when its hardware is implemented by a Zen1 module.
#include "zen1/emu/games.hpp"

#include "cps1.hpp"
#include "galaxian.hpp"
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
};
const Module kModules[] = {
    {cps1::moduleGames, cps1::moduleRomFiles, cps1::moduleCreate},
    {pacman::moduleGames, pacman::moduleRomFiles, pacman::moduleCreate},
    {galaxian::moduleGames, galaxian::moduleRomFiles, galaxian::moduleCreate},
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

std::unique_ptr<Machine> createMachine(const std::string& set, const std::filesystem::path& gamesDir,
                                       std::string* error) {
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
