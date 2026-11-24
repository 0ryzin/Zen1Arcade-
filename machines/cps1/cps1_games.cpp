// SPDX-License-Identifier: GPL-3.0-or-later
//
// CPS1 games supported by Zen1 Arcade.
// Board configurations, graphics bank maps, DIP defaults and ROM layouts
// are taken from MAME's cps1.cpp / cps1_v.cpp (BSD-3-Clause, copyright-holders:
// Paul Leaman). ROM sets must match current MAME naming (split or merged sets).
#include "cps1.hpp"

namespace zen1::cps1 {
namespace {

using emu::RomFile;
using emu::RomLoad;

const std::vector<GfxRange> kMapperSTF29 = {
    {GFX_SPRITES, 0x00000, 0x07fff, 0}, {GFX_SPRITES, 0x08000, 0x0ffff, 1},
    {GFX_SPRITES, 0x10000, 0x11fff, 2}, {GFX_SCROLL3, 0x02000, 0x03fff, 2},
    {GFX_SCROLL1, 0x04000, 0x04fff, 2}, {GFX_SCROLL2, 0x05000, 0x07fff, 2},
};
const std::vector<GfxRange> kMapperS224B = {
    {GFX_SPRITES, 0x0000, 0x43ff, 0}, {GFX_SCROLL1, 0x4400, 0x4bff, 0},
    {GFX_SCROLL3, 0x4c00, 0x5fff, 0}, {GFX_SCROLL2, 0x6000, 0x7fff, 0},
};
const std::vector<GfxRange> kMapperYI24B = {
    {GFX_SPRITES, 0x0000, 0x1fff, 0}, {GFX_SCROLL3, 0x2000, 0x3fff, 0},
    {GFX_SCROLL1, 0x4000, 0x47ff, 0}, {GFX_SCROLL2, 0x4800, 0x7fff, 0},
};

BoardConfig cpsB11() {  // CPS-B-11
    BoardConfig b;
    b.cpsbAddr = 0x32; b.cpsbValue = 0x0401;
    b.layerControl = 0x26; b.priority = {0x28, 0x2a, 0x2c, 0x2e}; b.paletteControl = 0x30;
    b.layerEnable = {0x08, 0x10, 0x20, 0x00, 0x00};
    return b;
}
BoardConfig cpsB21Def() {  // CPS-B-21 default
    BoardConfig b;
    b.cpsbAddr = 0x32; b.cpsbValue = -1;
    b.multFactor1 = 0x00; b.multFactor2 = 0x02; b.multLo = 0x04; b.multHi = 0x06;
    b.layerControl = 0x26; b.priority = {0x28, 0x2a, 0x2c, 0x2e}; b.paletteControl = 0x30;
    b.layerEnable = {0x02, 0x04, 0x08, 0x30, 0x30};
    return b;
}
BoardConfig cpsB04() {  // CPS-B-04
    BoardConfig b;
    b.cpsbAddr = 0x20; b.cpsbValue = 0x0004;
    b.layerControl = 0x2e; b.priority = {0x26, 0x30, 0x28, 0x32}; b.paletteControl = 0x2a;
    b.layerEnable = {0x02, 0x04, 0x08, 0x00, 0x00};
    return b;
}
BoardConfig cpsB05() {  // CPS-B-05
    BoardConfig b;
    b.cpsbAddr = 0x20; b.cpsbValue = 0x0005;
    b.layerControl = 0x28; b.priority = {0x2a, 0x2c, 0x2e, 0x30}; b.paletteControl = 0x32;
    b.layerEnable = {0x02, 0x08, 0x20, 0x14, 0x14};
    return b;
}

std::vector<RomFile> sound(const char* z80, std::uint32_t z80crc, const char* oki1, std::uint32_t c1,
                           const char* oki2, std::uint32_t c2) {
    return {
        {"audiocpu", z80, z80crc, 0x00000, 0x8000, RomLoad::Plain},
        {"audiocpu", nullptr, 0, 0x10000, 0x8000, RomLoad::Continue},
        {"oki", oki1, c1, 0x00000, 0x20000, RomLoad::Plain},
        {"oki", oki2, c2, 0x20000, 0x20000, RomLoad::Plain},
    };
}

std::vector<RomFile> concat(std::vector<RomFile> a, const std::vector<RomFile>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

std::vector<GameDef> makeDefs() {
    std::vector<GameDef> d;

    {   // Street Fighter II: The World Warrior (World 910522)
        GameDef g;
        g.info = {"sf2", "", "Street Fighter II: The World Warrior", "1991", "Capcom", "Capcom CPS1", 2, 6, 0};
        g.board = cpsB11();
        g.board.bankSizes = {0x8000, 0x8000, 0x8000, 0};
        g.board.mapper = kMapperSTF29;
        g.board.in2Addr = 0x36;
        g.dips = {0xff, 0xfc, 0x9f};
        g.roms = concat({
            {"maincpu", "sf2e_30g.11e", 0xfe39ee33, 0x00000, 0x20000, RomLoad::Byte16},
            {"maincpu", "sf2e_37g.11f", 0xfb92cd74, 0x00001, 0x20000, RomLoad::Byte16},
            {"maincpu", "sf2e_31g.12e", 0x69a0a301, 0x40000, 0x20000, RomLoad::Byte16},
            {"maincpu", "sf2e_38g.12f", 0x5e22db70, 0x40001, 0x20000, RomLoad::Byte16},
            {"maincpu", "sf2e_28g.9e", 0x8bf9f1e5, 0x80000, 0x20000, RomLoad::Byte16},
            {"maincpu", "sf2e_35g.9f", 0x626ef934, 0x80001, 0x20000, RomLoad::Byte16},
            {"maincpu", "sf2_29b.10e", 0xbb4af315, 0xc0000, 0x20000, RomLoad::Byte16},
            {"maincpu", "sf2_36b.10f", 0xc02a13eb, 0xc0001, 0x20000, RomLoad::Byte16},
            {"gfx", "sf2-5m.4a", 0x22c9cc8e, 0x000000, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-7m.6a", 0x57213be8, 0x000002, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-1m.3a", 0xba529b4f, 0x000004, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-3m.5a", 0x4b1b33a8, 0x000006, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-6m.4c", 0x2c7e2229, 0x200000, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-8m.6c", 0xb5548f17, 0x200002, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-2m.3c", 0x14b84312, 0x200004, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-4m.5c", 0x5e9cd89a, 0x200006, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-13m.4d", 0x994bfa58, 0x400000, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-15m.6d", 0x3e66ad9d, 0x400002, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-9m.3d", 0xc1befaa8, 0x400004, 0x80000, RomLoad::Word64},
            {"gfx", "sf2-11m.5d", 0x0627c831, 0x400006, 0x80000, RomLoad::Word64},
        }, sound("sf2_9.12a", 0xa4823a1b, "sf2_18.11c", 0x7f162009, "sf2_19.12c", 0xbeade53f));
        d.push_back(std::move(g));
    }
    {   // Street Fighter II': Champion Edition (World 920513)
        GameDef g;
        g.info = {"sf2ce", "sf2", "Street Fighter II': Champion Edition", "1992", "Capcom", "Capcom CPS1", 2, 6, 0};
        g.cpuClock = 12'000'000;
        g.board = cpsB21Def();
        g.board.bankSizes = {0x8000, 0x8000, 0x8000, 0};
        g.board.mapper = kMapperSTF29;  // S9263B: same ranges as STF29
        g.board.in2Addr = 0x36;
        g.dips = {0xff, 0xfc, 0x9f};
        g.roms = concat({
            {"maincpu", "s92e_23b.8f", 0x0aaa1a3a, 0x000000, 0x80000, RomLoad::WordSwap16},
            {"maincpu", "s92_22b.7f", 0x2bbe15ed, 0x080000, 0x80000, RomLoad::WordSwap16},
            {"maincpu", "s92_21a.6f", 0x925a7877, 0x100000, 0x80000, RomLoad::WordSwap16},
            {"gfx", "s92-1m.3a", 0x03b0d852, 0x000000, 0x80000, RomLoad::Word64},
            {"gfx", "s92-3m.5a", 0x840289ec, 0x000002, 0x80000, RomLoad::Word64},
            {"gfx", "s92-2m.4a", 0xcdb5f027, 0x000004, 0x80000, RomLoad::Word64},
            {"gfx", "s92-4m.6a", 0xe2799472, 0x000006, 0x80000, RomLoad::Word64},
            {"gfx", "s92-5m.7a", 0xba8a2761, 0x200000, 0x80000, RomLoad::Word64},
            {"gfx", "s92-7m.9a", 0xe584bfb5, 0x200002, 0x80000, RomLoad::Word64},
            {"gfx", "s92-6m.8a", 0x21e3f87d, 0x200004, 0x80000, RomLoad::Word64},
            {"gfx", "s92-8m.10a", 0xbefc47df, 0x200006, 0x80000, RomLoad::Word64},
            {"gfx", "s92-10m.3c", 0x960687d5, 0x400000, 0x80000, RomLoad::Word64},
            {"gfx", "s92-12m.5c", 0x978ecd18, 0x400002, 0x80000, RomLoad::Word64},
            {"gfx", "s92-11m.4c", 0xd6ec9a0a, 0x400004, 0x80000, RomLoad::Word64},
            {"gfx", "s92-13m.6c", 0xed2c67f6, 0x400006, 0x80000, RomLoad::Word64},
        }, sound("s92_09.11a", 0x08f6b60e, "s92_18.11c", 0x7f162009, "s92_19.12c", 0xbeade53f));
        d.push_back(std::move(g));
    }
    {   // Final Fight (World, set 1)
        GameDef g;
        g.info = {"ffight", "", "Final Fight", "1989", "Capcom", "Capcom CPS1", 2, 2, 0};
        g.board = cpsB04();
        g.board.bankSizes = {0x8000, 0, 0, 0};
        g.board.mapper = kMapperS224B;
        g.dips = {0xff, 0xf4, 0x9f};
        g.roms = concat({
            {"maincpu", "ff_36.11f", 0xf9a5ce83, 0x00000, 0x20000, RomLoad::Byte16},
            {"maincpu", "ff_42.11h", 0x65f11215, 0x00001, 0x20000, RomLoad::Byte16},
            {"maincpu", "ff_37.12f", 0xe1033784, 0x40000, 0x20000, RomLoad::Byte16},
            {"maincpu", "ffe_43.12h", 0x995e968a, 0x40001, 0x20000, RomLoad::Byte16},
            {"maincpu", "ff-32m.8h", 0xc747696e, 0x80000, 0x80000, RomLoad::WordSwap16},
            {"gfx", "ff-5m.7a", 0x9c284108, 0x000000, 0x80000, RomLoad::Word64},
            {"gfx", "ff-7m.9a", 0xa7584dfb, 0x000002, 0x80000, RomLoad::Word64},
            {"gfx", "ff-1m.3a", 0x0b605e44, 0x000004, 0x80000, RomLoad::Word64},
            {"gfx", "ff-3m.5a", 0x52291cd2, 0x000006, 0x80000, RomLoad::Word64},
        }, sound("ff_09.12b", 0xb8367eb5, "ff_18.11c", 0x375c66e7, "ff_19.12c", 0x1ef137f9));
        d.push_back(std::move(g));
    }
    {   // 1941: Counter Attack (World 900227) - vertical game
        GameDef g;
        g.info = {"1941", "", "1941: Counter Attack", "1990", "Capcom", "Capcom CPS1", 2, 2, 270};
        g.board = cpsB05();
        g.board.bankSizes = {0x8000, 0, 0, 0};
        g.board.mapper = kMapperYI24B;
        g.dips = {0xff, 0xfc, 0x9f};
        g.roms = concat({
            {"maincpu", "41em_30.11f", 0x4249ec61, 0x00000, 0x20000, RomLoad::Byte16},
            {"maincpu", "41em_35.11h", 0xddbee5eb, 0x00001, 0x20000, RomLoad::Byte16},
            {"maincpu", "41em_31.12f", 0x584e88e5, 0x40000, 0x20000, RomLoad::Byte16},
            {"maincpu", "41em_36.12h", 0x3cfc31d0, 0x40001, 0x20000, RomLoad::Byte16},
            {"maincpu", "41-32m.8h", 0x4e9648ca, 0x80000, 0x80000, RomLoad::WordSwap16},
            {"gfx", "41-5m.7a", 0x01d1cb11, 0x000000, 0x80000, RomLoad::Word64},
            {"gfx", "41-7m.9a", 0xaeaa3509, 0x000002, 0x80000, RomLoad::Word64},
            {"gfx", "41-1m.3a", 0xff77985a, 0x000004, 0x80000, RomLoad::Word64},
            {"gfx", "41-3m.5a", 0x983be58f, 0x000006, 0x80000, RomLoad::Word64},
        }, sound("41_9.12b", 0x0f9d8527, "41_18.11c", 0xd1f15aeb, "41_19.12c", 0x15aec3a6));
        d.push_back(std::move(g));
    }
    return d;
}

std::uint32_t gfxSize(const GameDef& g) {
    std::uint32_t end = 0;
    for (const auto& r : g.roms)
        if (std::string(r.region) == "gfx") end = std::max(end, (r.offset & ~7u) + r.length * 4);
    return end;
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

// ---- Engine-wide game registry (currently: CPS1 module only) ----

namespace zen1::emu {

const std::vector<GameInfo>& supportedGames() {
    static const std::vector<GameInfo> list = [] {
        std::vector<GameInfo> v;
        for (const auto& g : cps1::gameDefs()) v.push_back(g.info);
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
    const auto* g = cps1::findGameDef(set);
    return g ? g->roms : std::vector<RomFile>{};
}

std::unique_ptr<Machine> createMachine(const std::string& set, const std::filesystem::path& gamesDir,
                                       std::string* error) {
    const auto* g = cps1::findGameDef(set);
    if (!g) {
        if (error) *error = "'" + set + "' is not supported by the Zen1 engine";
        return nullptr;
    }
    const std::vector<RomRegion> regions = {
        {"maincpu", 0x400000}, {"gfx", cps1::gfxSize(*g)}, {"audiocpu", 0x18000}, {"oki", 0x40000}};
    auto loaded = loadRomSet(gamesDir, g->info.set, g->info.parent, regions, g->roms);
    if (!loaded.ok) {
        if (error) {
            *error = "ROM set '" + set + "' is incomplete or a different version. Missing or wrong:";
            for (const auto& m : loaded.missing) *error += "\n  " + m;
        }
        return nullptr;
    }
    return std::make_unique<cps1::Cps1Machine>(*g, std::move(loaded.regions));
}

}  // namespace zen1::emu
