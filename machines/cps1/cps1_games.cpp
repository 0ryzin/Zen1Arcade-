// SPDX-License-Identifier: GPL-3.0-or-later
//
// CPS1 games supported by Zen1 Arcade.
// Board configurations, graphics bank maps, DIP defaults and ROM layouts
// come from MAME's cps1.cpp / cps1_v.cpp (BSD-3-Clause, copyright-holders:
// Paul Leaman) through tools/gen_cps1_games.py -> cps1_games_data.inc.
// ROM sets must match current MAME naming (split or merged sets).
#include "cps1.hpp"

namespace zen1::cps1 {
namespace {

using emu::RomFile;
using emu::RomLoad;

std::vector<GameDef> makeDefs() {
    std::vector<GameDef> d;
#include "cps1_games_data.inc"

    return d;
}

std::uint32_t gfxSize(const GameDef& g) {
    std::uint32_t end = 0;
    for (const auto& r : g.roms)
        if (std::string(r.region) == "gfx")
            end = std::max(end, (r.offset & ~7u) + r.length * (r.load == emu::RomLoad::Byte64 ? 8 : 4));
    return g.gfxRegionSize ? std::max<std::uint32_t>(g.gfxRegionSize, end) : end;
}

}  // namespace

const std::vector<GameDef>& gameDefs() {
    static const std::vector<GameDef> defs = makeDefs();
    return defs;
}

const GameDef* findGameDef(const std::string& set) {
    for (const auto& g : gameDefs())
        if (g.info.set == set) return &g;
    return nullptr;
}

}  // namespace zen1::cps1

// ---- Module interface used by machines/registry.cpp ----

namespace zen1::cps1 {

std::vector<emu::GameInfo> moduleGames() {
    std::vector<emu::GameInfo> v;
    for (const auto& g : gameDefs()) v.push_back(g.info);
    return v;
}

std::vector<emu::RomFile> moduleRomFiles(const std::string& set) {
    const auto* g = findGameDef(set);
    return g ? g->roms : std::vector<emu::RomFile>{};
}

std::unique_ptr<emu::Machine> moduleCreate(const std::string& set, const std::filesystem::path& gamesDir,
                                           std::string* error) {
    const auto* g = findGameDef(set);
    if (!g) return nullptr;
    const std::vector<emu::RomRegion> regions = {
        {"maincpu", 0x400000}, {"gfx", gfxSize(*g)}, {"audiocpu", 0x18000}, {"oki", 0x40000}};
    auto loaded = emu::loadRomSet(gamesDir, g->info.set, g->info.parent, regions, g->roms);
    if (!loaded.ok) {
        if (error) {
            *error = "ROM set '" + set + "' is incomplete or a different version. Missing or wrong:";
            for (const auto& m : loaded.missing) *error += "\n  " + m;
        }
        return nullptr;
    }
    return std::make_unique<Cps1Machine>(*g, std::move(loaded.regions));
}

}  // namespace zen1::cps1
