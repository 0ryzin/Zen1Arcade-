// SPDX-License-Identifier: GPL-3.0-or-later
// Pac-Man hardware tests with a hand-written Z80 program and synthetic
// graphics/PROMs. No Namco ROM data is used.
#include "pacman.hpp"
#include "zen1/emu/games.hpp"
#include "zen1_test.hpp"

using namespace zen1;

namespace {
emu::RegionMap regions() {
    emu::RegionMap r;
    r["maincpu"].assign(0x4000, 0);
    r["gfx1"].assign(0x2000, 0);
    r["proms"].assign(0x120, 0);
    r["namco"].assign(0x100, 0);
    auto& m = r["maincpu"];
    // 0x0000: IM 2 ; LD A,0x10 ; OUT (0),A ; LD A,1 ; LD (0x5000),A ; EI ; JR $
    const std::uint8_t prog[] = {0xed, 0x5e, 0x3e, 0x10, 0xd3, 0x00, 0x3e, 0x01, 0x32, 0x00, 0x50, 0xfb, 0x18, 0xfe};
    std::copy(std::begin(prog), std::end(prog), m.begin());
    // Vector table: I=0, vector 0x10 -> handler at 0x0100.
    m[0x10] = 0x00; m[0x11] = 0x01;
    // 0x0100: LD HL,(0x4c00) ; INC HL ; LD (0x4c00),HL ; EI ; RETI
    const std::uint8_t isr[] = {0x2a, 0x00, 0x4c, 0x23, 0x22, 0x00, 0x4c, 0xfb, 0xed, 0x4d};
    std::copy(std::begin(isr), std::end(isr), m.begin() + 0x100);
    // Palette: colour 1 = pure red (r bits 0-2 set); lookup colour 0, pen 3 -> 1.
    r["proms"][1] = 0x07;
    r["proms"][0x20 + 3] = 1;
    // Tile 1: every pixel pen 3 (both planes set in all bytes).
    for (int i = 0; i < 16; ++i) r["gfx1"][16 + static_cast<size_t>(i)] = 0xff;
    // Waveform 0: square.
    for (int i = 0; i < 32; ++i) r["namco"][static_cast<size_t>(i)] = i < 16 ? 15 : 0;
    return r;
}
}  // namespace

TEST("Pac-Man: Z80 boots in IM 2, takes one VBLANK interrupt per frame") {
    emu::GameInfo info;
    info.rotation = 90;
    pacman::PacmanMachine m(info, pacman::Variant::Pacman, regions(), 0xc9);
    for (int f = 0; f < 5; ++f) m.runFrame();
    CHECK_EQ(m.read(0x4c00) | (m.read(0x4c01) << 8), 5);
    CHECK_EQ(m.video().rotation, 90);
    CHECK_EQ(m.read(0x5080), 0xc9);  // DIP switches
}

TEST("Pac-Man: tiles use the scan-row layout and the PROM colours") {
    emu::GameInfo info;
    pacman::PacmanMachine m(info, pacman::Variant::Pacman, regions(), 0xc9);
    // Screen column 2, row 0 is video RAM offset 0x3a0 + ... : column 2 -> c=0, row 0 -> r=2: index 0 + (2 << 5) = 0x40.
    m.write(0x4040, 1);
    m.write(0x4440, 0);
    m.render();
    const std::uint32_t red = static_cast<std::uint32_t>(0x21 + 0x47 + 0x97) << 16;
    CHECK_EQ(m.frame()[0 * 288 + 2 * 8], red);
    CHECK_EQ(m.frame()[7 * 288 + 2 * 8 + 7], red);
    CHECK(m.frame()[8 * 288 + 2 * 8] != red);
}

TEST("Pac-Man: the waveform sound generator makes a tone only when enabled") {
    emu::GameInfo info;
    pacman::PacmanMachine m(info, pacman::Variant::Pacman, regions(), 0xc9);
    m.write(0x5045, 0);     // voice 0 waveform 0
    m.write(0x5053, 1);     // frequency nibble 3
    m.write(0x5055, 15);    // volume
    std::vector<std::int16_t> a;
    m.runFrame();
    m.takeAudio(a);
    bool any = false;
    for (auto s : a) any = any || s != 0;
    CHECK(!any);            // sound disabled at reset
    m.write(0x5001, 1);     // sound enable latch
    m.runFrame();
    a.clear();
    m.takeAudio(a);
    CHECK(a.size() > 1500u);
    int peak = 0;
    for (auto s : a) peak = std::max(peak, static_cast<int>(s));
    CHECK(peak > 1000);
}

TEST("Pac-Man family is in the game list with 4-way sticks and vertical screens") {
    const auto* g = emu::findGame("mspacman");
    CHECK(g != nullptr);
    CHECK_EQ(g->joystick, std::string("4-way"));
    CHECK_EQ(g->rotation, 90);
    CHECK_EQ(emu::findGame("pacman")->parent, std::string("puckman"));
}
