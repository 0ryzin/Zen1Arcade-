// SPDX-License-Identifier: GPL-3.0-or-later
// Capcom 1942 hardware tests with hand-written Z80 code, synthetic PROMs and
// graphics. No Capcom data is used.
#include "c1942.hpp"
#include "zen1/emu/games.hpp"
#include "zen1_test.hpp"

#include <cstdio>
#include <cstdlib>

using namespace zen1;

namespace {
emu::RegionMap regions1942() {
    emu::RegionMap r;
    r["maincpu"].assign(0x20000, 0);
    r["audiocpu"].assign(0x10000, 0);
    r["gfx1"].assign(0x2000, 0);
    r["gfx2"].assign(0xc000, 0);
    r["gfx3"].assign(0x10000, 0);
    r["palproms"].assign(0x300, 0);
    r["charprom"].assign(0x100, 0);
    r["tileprom"].assign(0x100, 0);
    r["sprprom"].assign(0x100, 0);
    r["irqprom"].assign(0x100, 0);
    auto& m = r["maincpu"];
    const std::uint8_t boot[] = {0x31, 0x00, 0xf0, 0xed, 0x46, 0xfb, 0x18, 0xfe};  // LD SP; IM 0; EI; JR $
    std::copy(std::begin(boot), std::end(boot), m.begin());
    const std::uint8_t rst08[] = {0x21, 0x00, 0xe0, 0x34, 0xfb, 0xc9};               // INC (E000); EI; RET
    const std::uint8_t rst10[] = {0x21, 0x01, 0xe0, 0x34, 0xfb, 0xc9};               // INC (E001); EI; RET
    std::copy(std::begin(rst08), std::end(rst08), m.begin() + 0x08);
    std::copy(std::begin(rst10), std::end(rst10), m.begin() + 0x10);
    auto& s = r["audiocpu"];
    const std::uint8_t sboot[] = {0xc3, 0x40, 0x00};
    std::copy(std::begin(sboot), std::end(sboot), s.begin());
    const std::uint8_t sirq[] = {0x21, 0x00, 0x40, 0x34, 0xfb, 0xc9};
    std::copy(std::begin(sirq), std::end(sirq), s.begin() + 0x38);
    const std::uint8_t smain[] = {0x31, 0x00, 0x48, 0xed, 0x56, 0xfb,
                                  0x3e, 0x07, 0x32, 0x00, 0x80, 0x3e, 0x3e, 0x32, 0x01, 0x80,   // mixer: tone A
                                  0x3e, 0x08, 0x32, 0x00, 0x80, 0x3e, 0x0f, 0x32, 0x01, 0x80,   // volume A
                                  0x3e, 0x00, 0x32, 0x00, 0x80, 0x3e, 0x80, 0x32, 0x01, 0x80,   // period
                                  0x18, 0xfe};
    std::copy(std::begin(smain), std::end(smain), s.begin() + 0x40);
    auto& irq = r["irqprom"];
    irq[109] = 0x09;  // RST 08h
    irq[240] = 0x0a;  // RST 10h (vblank)
    for (int l : {16, 80, 144, 208}) irq[static_cast<size_t>(l)] |= 0x04;  // sound
    return r;
}
}  // namespace

TEST("1942: PROM-timed interrupts (RST 08h, RST 10h), four sound IRQs a frame, AY tone") {
    c1942::C1942Machine m(*emu::findGame("1942"), regions1942(), {0xf7, 0xff});
    std::vector<std::int16_t> audio;
    for (int f = 0; f < 10; ++f) {
        m.runFrame();
        m.takeAudio(audio);
    }
    CHECK_EQ(m.read(0xe000), 10);
    CHECK_EQ(m.read(0xe001), 10);
    CHECK_EQ(static_cast<int>(m.soundRead(0x4000)), 40);
    CHECK(std::abs(m.video().refreshHz - 59.64) < 0.05);
    CHECK(audio.size() > 2 * 7700 && audio.size() < 2 * 7900);  // 46875 Hz for 10 frames
    int peak = 0;
    for (size_t i = audio.size() / 2; i < audio.size(); ++i) peak = std::max(peak, std::abs(static_cast<int>(audio[i])));
    CHECK(peak > 500);
}

TEST("1942: text layer over the scrolling background, colours through the PROMs") {
    auto r = regions1942();
    // Char 1: every pixel pen 3 (both planes set).
    for (int i = 16; i < 32; ++i) r["gfx1"][static_cast<size_t>(i)] = 0xff;
    r["charprom"][1 * 4 + 3] = 0x05;      // char colour 1, pen 3 -> palette 0x85
    r["palproms"][0x85] = 0x0f;            // red full
    r["tileprom"][0] = 0x02;               // background pen 0 -> palette 0x02
    r["palproms"][0x200 + 0x02] = 0x0f;    // blue full
    c1942::C1942Machine m(*emu::findGame("1942"), std::move(r), {0xf7, 0xff});
    m.write(0xd000 + 4 * 32 + 3, 1);       // row 4, column 3
    m.write(0xd400 + 4 * 32 + 3, 1);       // colour 1
    for (int l = 0; l < 262; ++l) m.renderLine(l);
    const std::uint32_t* f = m.frame();
    CHECK_EQ(f[(4 * 8 - 16) * 256 + 3 * 8], 0xff0000u);
    CHECK_EQ(f[(4 * 8 - 16) * 256 + 100], 0x0000ffu);
    CHECK_EQ(emu::findGame("1942")->rotation, 270);
    CHECK_EQ(emu::findGame("1942a")->parent, std::string("1942"));
}
