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
#include "cps2_games_data.inc"

    // Every CPS-2 B-board has the same CPS-B-21 at its default settings
    // (MAME CPS_B_21_DEF) and maps all tiles through one bank (mapper_cps2).
    for (auto& g : d) {
        if (!g.cps2) continue;
        auto& b = g.board;
        b.cpsbAddr = 0x32; b.cpsbValue = -1;
        b.multFactor1 = 0x00; b.multFactor2 = 0x02; b.multLo = 0x04; b.multHi = 0x06;
        b.layerControl = 0x26; b.priority = {0x28, 0x2a, 0x2c, 0x2e}; b.paletteControl = 0x30;
        b.layerEnable = {0x02, 0x04, 0x08, 0x30, 0x30};
        b.bankSizes = {0x20000, 0x20000, 0, 0};
        b.mapper = {{GFX_SCROLL1 | GFX_SCROLL2 | GFX_SCROLL3, 0x00000, 0x1ffff, 1}};
        g.cpuClock = 11'800'000;
    }
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
    std::vector<emu::RomRegion> regions = {
        {"maincpu", 0x400000}, {"gfx", gfxSize(*g)}, {"audiocpu", 0x18000}, {"oki", 0x40000}};
    if (g->cps2)
        regions = {{"maincpu", 0x400000}, {"gfx", gfxSize(*g)}, {"audiocpu", 0x50000},
                   {"qsound", std::max<std::uint32_t>(g->qsoundRegionSize, 0x10000)}, {"key", 0x20, 0xff}};
    auto loaded = emu::loadRomSet(gamesDir, g->info.set, g->info.parent, regions, g->roms);
    if (!loaded.ok) {
        if (error) {
            *error = "ROM set '" + set + "' is incomplete or a different version. Missing or wrong:";
            for (const auto& m : loaded.missing) *error += "\n  " + m;
        }
        return nullptr;
    }
    auto m = std::make_unique<Cps1Machine>(*g, std::move(loaded.regions));
    if (g->cps2) {
        // QSound DSP tables from the user's qsound.zip (MAME): optional.
        const std::vector<emu::RomFile> dsp = {{"dsp", "dl-1425.bin", 0xd6cf5ef5, 0, 0x2000, emu::RomLoad::Plain}};
        if (emu::findSetZip(gamesDir, "qsound")) {
            auto d = emu::loadRomSet(gamesDir, "qsound", "", {{"dsp", 0x2000}}, dsp);
            if (d.ok) m->setQsoundDspRom(d.regions["dsp"]);
        }
    }
    return m;
}

}  // namespace zen1::cps1
