// SPDX-License-Identifier: GPL-3.0-or-later
// Galaxian hardware tests with a hand-written Z80 program and synthetic
// graphics. No Namco ROM data is used.
#include "galaxian.hpp"
#include "zen1/emu/games.hpp"
#include "zen1_test.hpp"

using namespace zen1;

namespace {
emu::RegionMap regions() {
    emu::RegionMap r;
    r["maincpu"].assign(0x4000, 0);
    r["gfx1"].assign(0x1000, 0);
    r["proms"].assign(0x20, 0);
    auto& m = r["maincpu"];
    // LD SP,0x4400 ; LD A,1 ; LD (0x7001),A ; JR $
    const std::uint8_t prog[] = {0x31, 0x00, 0x44, 0x3e, 0x01, 0x32, 0x01, 0x70, 0x18, 0xfe};
    std::copy(std::begin(prog), std::end(prog), m.begin());
    // NMI at 0x66: LD HL,(0x4000) ; INC HL ; LD (0x4000),HL ; RETN
    const std::uint8_t nmi[] = {0x2a, 0x00, 0x40, 0x23, 0x22, 0x00, 0x40, 0xed, 0x45};
    std::copy(std::begin(nmi), std::end(nmi), m.begin() + 0x66);
    r["proms"][3] = 0x07;                                     // colour 0, pen 3 = red
    for (int i = 0; i < 8; ++i) {                             // tile 1: every pixel pen 3
        r["gfx1"][8 + static_cast<size_t>(i)] = 0xff;
        r["gfx1"][0x800 + 8 + static_cast<size_t>(i)] = 0xff;
    }
    return r;
}
}  // namespace

TEST("Galaxian: VBLANK NMI only when enabled, once per frame") {
    emu::GameInfo info;
    info.rotation = 90;
    galaxian::GalaxianMachine m(info, regions(), {0, 0, 0});
    for (int f = 0; f < 6; ++f) m.runFrame();
    const int count = m.read(0x4000) | (m.read(0x4001) << 8);
    CHECK(count >= 5 && count <= 6);
    CHECK_EQ(m.video().rotation, 90);
}

TEST("Galaxian: tiles scroll per column and use the PROM colours") {
    emu::GameInfo info;
    galaxian::GalaxianMachine m(info, regions(), {0, 0, 0});
    m.write(0x5000 + 2 * 32 + 5, 1);   // tile row 2, column 5 -> screen rows 0..7 (16 lines hidden at top)
    m.render();
    const std::uint32_t red = static_cast<std::uint32_t>(0x21 + 0x47 + 0x97) << 16;
    CHECK_EQ(m.frame()[0 * 256 + 5 * 8], red);
    CHECK(m.frame()[8 * 256 + 5 * 8] != red);
    m.write(0x5800 + 5 * 2, 8);         // column 5 scrolled by 8 lines
    m.render();
    CHECK(m.frame()[0 * 256 + 5 * 8] != red);
    CHECK_EQ(m.frame()[0 * 256 + 4 * 8], 0u);  // other columns unaffected
}

TEST("Galaxian family is in the game list (2-way, vertical, one button)") {
    const auto* g = emu::findGame("galaxian");
    CHECK(g != nullptr);
    CHECK_EQ(g->joystick, std::string("2-way"));
    CHECK_EQ(g->buttons, 1);
    CHECK_EQ(emu::findGame("superg")->parent, std::string("galaxian"));
}
