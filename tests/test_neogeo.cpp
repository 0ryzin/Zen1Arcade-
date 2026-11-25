// SPDX-License-Identifier: GPL-3.0-or-later
// Neo Geo machine tests with a hand-written 68000 "BIOS", a hand-written
// Z80 sound program and synthetic graphics. No SNK data is used.
#include "neogeo.hpp"
#include "zen1/emu/games.hpp"
#include "zen1_test.hpp"

using namespace zen1;

namespace {
void put16(std::vector<std::uint8_t>& m, std::uint32_t a, std::uint16_t v) {
    m[a] = static_cast<std::uint8_t>(v >> 8);
    m[a + 1] = static_cast<std::uint8_t>(v);
}
void put32(std::vector<std::uint8_t>& m, std::uint32_t a, std::uint32_t v) {
    put16(m, a, static_cast<std::uint16_t>(v >> 16));
    put16(m, a + 2, static_cast<std::uint16_t>(v));
}

emu::RegionMap regions() {
    emu::RegionMap r;
    r["maincpu"].assign(0x100000, 0);
    r["bios"].assign(0x20000, 0);
    r["fixed"].assign(0x20000, 0);
    r["fixedbios"].assign(0x20000, 0);
    r["zoomy"].assign(0x20000, 0);
    r["audiobios"].assign(0x20000, 0);
    r["audiocpu"].assign(0x20000, 0);
    r["adpcma"].assign(0x100000, 0);
    r["sprites"].assign(0x100000, 0);
    auto& b = r["bios"];
    put32(b, 0x00, 0x0010f300);  // SSP
    put32(b, 0x04, 0x00c00400);  // PC (BIOS space)
    put32(b, 0x64, 0x00c00500);  // level 1: VBLANK
    put32(b, 0x68, 0x00c00600);  // level 2: display position (raster)
    put32(b, 0x6c, 0x00c00700);  // level 3: cold boot
    // 0x400: MOVE.W #$2000,SR ; BRA.S *
    put16(b, 0x400, 0x46fc); put16(b, 0x402, 0x2000); put16(b, 0x404, 0x60fe);
    // 0x500: ADDQ.W #1,$10F000 ; MOVE.W #4,$3C000C ; RTE
    put16(b, 0x500, 0x5279); put32(b, 0x502, 0x0010f000);
    put16(b, 0x506, 0x33fc); put16(b, 0x508, 0x0004); put32(b, 0x50a, 0x003c000c);
    put16(b, 0x50e, 0x4e73);
    // 0x600: ADDQ.W #1,$10F002 ; MOVE.W #2,$3C000C ; RTE
    put16(b, 0x600, 0x5279); put32(b, 0x602, 0x0010f002);
    put16(b, 0x606, 0x33fc); put16(b, 0x608, 0x0002); put32(b, 0x60a, 0x003c000c);
    put16(b, 0x60e, 0x4e73);
    // 0x700: ADDQ.W #1,$10F004 ; MOVE.W #1,$3C000C ; RTE
    put16(b, 0x700, 0x5279); put32(b, 0x702, 0x0010f004);
    put16(b, 0x706, 0x33fc); put16(b, 0x708, 0x0001); put32(b, 0x70a, 0x003c000c);
    put16(b, 0x70e, 0x4e73);

    // Z80 (sound BIOS): OUT (8),A enables NMI ; JR $ ; NMI: IN A,(0) ; INC A ; OUT (0Ch),A ; RETN
    auto& z = r["audiobios"];
    const std::uint8_t zp[] = {0x31, 0xff, 0xff, 0xd3, 0x08, 0x18, 0xfe};
    std::copy(std::begin(zp), std::end(zp), z.begin());
    const std::uint8_t nmi[] = {0xdb, 0x00, 0x3c, 0xd3, 0x0c, 0xed, 0x45};
    std::copy(std::begin(nmi), std::end(nmi), z.begin() + 0x66);

    // Zoom table for full size (0xff): line l -> tile l/16, row l%16.
    for (int l = 0; l < 256; ++l) r["zoomy"][static_cast<size_t>(0xff << 8 | l)] = static_cast<std::uint8_t>(l);
    // Sprite tile 0: every pixel pen 1 (plane 0 bytes set).
    for (int i = 0; i < 0x80; i += 4) r["sprites"][static_cast<size_t>(i)] = 0xff;
    // Fix tile 1 in the BIOS fix ROM: every pixel pen 1.
    for (int i = 0; i < 32; ++i) r["fixedbios"][static_cast<size_t>(32 + i)] = 0x11;
    return r;
}
}  // namespace

TEST("Neo Geo: BIOS boots, cold-boot IRQ3 then one VBLANK IRQ1 per frame, acknowledged through the LSPC") {
    emu::GameInfo info;
    neogeo::NeoGeoMachine m(info, regions());
    for (int f = 0; f < 10; ++f) m.runFrame();
    CHECK_EQ(m.read16(0x10f004), 1);   // IRQ3 once
    CHECK_EQ(m.read16(0x10f000), 10);  // VBLANK per frame
    CHECK_EQ(m.read16(0x10f002), 0);   // no raster interrupt unless programmed
    CHECK_EQ(m.ipl(), 0);
}

TEST("Neo Geo: display-position timer raises IRQ2 repeatedly when set to repeat") {
    emu::GameInfo info;
    neogeo::NeoGeoMachine m(info, regions());
    m.runFrame();
    // Every 384*20 pixels (20 lines): enable + relative load + repeat.
    m.write16(0x3c0006, 0x00b0);
    m.write16(0x3c0008, 0x0000);
    m.write16(0x3c000a, static_cast<std::uint16_t>(384 * 20 - 1));
    m.runFrame();
    const int n = m.read16(0x10f002);
    CHECK(n >= 12 && n <= 14);  // 264 lines / 20
}

TEST("Neo Geo: sound command reaches the Z80 by NMI and the reply comes back") {
    emu::GameInfo info;
    neogeo::NeoGeoMachine m(info, regions());
    m.runFrame();
    m.write8(0x320000, 0x41);
    m.runFrame();
    CHECK_EQ(m.read16(0x320000) >> 8, 0x42);
}

TEST("Neo Geo: a sprite and the fix layer are drawn with the right palette colours") {
    emu::GameInfo info;
    neogeo::NeoGeoMachine m(info, regions());
    // Palette 1 pen 1 = pure red (5-bit 31), palette 2 pen 1 = pure green.
    m.write16(0x400000 + (1 * 16 + 1) * 2, 0x4f00);
    m.write16(0x400000 + (2 * 16 + 1) * 2, 0x20f0);
    auto& v = m.vram();
    // Sprite 1: tile code 0, palette 1, full size, 1 tile tall, at (40, 50).
    v[1 << 6] = 0x0000;
    v[(1 << 6) + 1] = 0x0100;
    v[0x8000 | 1] = 0x0fff;
    v[0x8200 | 1] = static_cast<std::uint16_t>(((0x200 - 50) << 7) | 1);
    v[0x8400 | 1] = static_cast<std::uint16_t>(40 << 7);
    // Fix layer: column 3, row 4 -> tile 1, palette 2.
    v[0x7000 + 3 * 32 + 4] = 0x2001;
    for (int l = 0; l < 264; ++l) m.renderLine(l);
    const std::uint32_t* f = m.frame();
    CHECK_EQ(f[(50 - 16) * 320 + 40], 0xff0000u);
    CHECK_EQ(f[(50 - 16 + 15) * 320 + 55], 0xff0000u);
    CHECK(f[(50 - 16 + 16) * 320 + 40] != 0xff0000u);
    CHECK_EQ(f[(4 * 8 - 16) * 320 + 3 * 8], 0x00ff00u);
}

TEST("Neo Geo: 120+ standard MVS games listed, needing neogeo.zip") {
    const auto* g = emu::findGame("mslug");
    CHECK(g != nullptr);
    CHECK_EQ(g->bios, std::string("neogeo"));
    CHECK_EQ(g->buttonNames.size(), static_cast<size_t>(3));
    CHECK(emu::findGame("kof97") != nullptr);
    CHECK(emu::findGame("garou") == nullptr);  // encrypted: not supported yet
    int n = 0;
    for (const auto& x : emu::supportedGames()) n += x.system == "SNK Neo Geo MVS";
    CHECK(n >= 120);
}
