// SPDX-License-Identifier: GPL-3.0-or-later
// CPS-2 machine tests with hand-written 68000 / Z80 code and synthetic
// graphics and samples. No Capcom data is used (no key file: the program
// runs unencrypted, like homebrew on a "dead" board).
#include "cps1.hpp"
#include "cps2crypt.hpp"
#include "zen1/emu/games.hpp"
#include "zen1_test.hpp"

#include <cstddef>
#include <cmath>

using namespace zen1;

namespace {

emu::RegionMap cps2Regions() {
    emu::RegionMap r;
    r["maincpu"].assign(0x400000, 0);
    r["gfx"].assign(0x400000, 0);
    r["audiocpu"].assign(0x50000, 0);
    r["qsound"].assign(0x10000, 0);
    r["key"].assign(0x20, 0xff);  // no key: not decrypted
    r["oki"].assign(0x10, 0);
    r["audiocpu"][0] = 0x76;  // HALT
    return r;
}

void put16(std::vector<std::uint8_t>& m, std::uint32_t a, std::uint16_t v) {
    m[a] = static_cast<std::uint8_t>(v >> 8);
    m[a + 1] = static_cast<std::uint8_t>(v);
}
void put32(std::vector<std::uint8_t>& m, std::uint32_t a, std::uint32_t v) {
    put16(m, a, static_cast<std::uint16_t>(v >> 16));
    put16(m, a + 2, static_cast<std::uint16_t>(v));
}

const cps1::GameDef& sfa3() { return *cps1::findGameDef("sfa3"); }

}  // namespace

TEST("CPS-2: game list has the CPS-2 library with QSound and 6-button games") {
    const auto* g = emu::findGame("ssf2t");
    CHECK(g != nullptr);
    CHECK_EQ(g->system, std::string("Capcom CPS-2"));
    CHECK_EQ(g->buttons, 6);
    CHECK(emu::findGame("sfa3") != nullptr);
    CHECK(emu::findGame("ddtod") != nullptr);
    CHECK_EQ(emu::findGame("19xx")->rotation, 270);
    CHECK(emu::findGame("pzloop2") == nullptr);  // paddle panel
    int n = 0;
    for (const auto& x : emu::supportedGames()) n += x.system == "Capcom CPS-2";
    CHECK(n >= 200);
    // Every CPS-2 set needs its key file
    bool keys = true;
    for (const auto& f : cps1::findGameDef("sfa3")->roms) keys = keys || std::string(f.region) == "key";
    CHECK(keys);
}

TEST("CPS-2: vblank IRQ 2 every frame and raster IRQ 4 from the CPS-B-21 line counter") {
    auto r = cps2Regions();
    auto& rom = r["maincpu"];
    put32(rom, 0x00, 0x00fffff0);
    put32(rom, 0x04, 0x00000400);
    put32(rom, 0x68, 0x00000500);  // level 2
    put32(rom, 0x70, 0x00000600);  // level 4
    put32(rom, 0x78, 0x00000700);  // level 6 (both at once)
    put16(rom, 0x400, 0x46fc); put16(rom, 0x402, 0x2000); put16(rom, 0x404, 0x60fe);
    put16(rom, 0x500, 0x5279); put32(rom, 0x502, 0x00ff8000); put16(rom, 0x506, 0x4e73);
    put16(rom, 0x600, 0x5279); put32(rom, 0x602, 0x00ff8002); put16(rom, 0x606, 0x4e73);
    put16(rom, 0x700, 0x5279); put32(rom, 0x702, 0x00ff8004); put16(rom, 0x706, 0x4e73);
    cps1::Cps1Machine m(sfa3(), std::move(r));
    m.write16(0x804150, 100);    // raster counter 1 reload: IRQ on line 100
    m.write16(0x804152, 0x1ff);  // counter 2: never reaches 0 in a frame
    for (int f = 0; f < 5; ++f) m.runFrame();
    CHECK_EQ(m.read16(0xff8000), static_cast<std::uint16_t>(5));
    CHECK_EQ(m.read16(0xff8002), static_cast<std::uint16_t>(5));
    CHECK_EQ(m.read16(0xff8004), static_cast<std::uint16_t>(0));
}

TEST("CPS-2: inputs, object RAM banks and QSound shared RAM") {
    cps1::Cps1Machine m(sfa3(), cps2Regions());
    emu::CabinetInput in;
    in.player[0].left = true;
    in.player[0].button[0] = true;  // LP: IN0
    in.player[0].button[5] = true;  // HK: IN1 bit 2
    in.player[1].button[5] = true;  // P2 HK: IN2 bit 14
    in.player[1].coin = true;
    m.setInput(in);
    CHECK_EQ(m.read16(0x804000), static_cast<std::uint16_t>(0xffff & ~0x12));
    CHECK_EQ(m.read16(0x804010), static_cast<std::uint16_t>(0xffff & ~0x04));
    CHECK_EQ(m.read16(0x804020) & 0x6000, 0);  // P2 HK and coin 2 low

    m.write16(0x8040e0, 0);
    m.write16(0x708000, 0x1234);  // CPU side writes the bank that is not displayed
    m.write16(0x8040e0, 1);
    CHECK(m.read16(0x708000) != 0x1234);
    m.write16(0x8040e0, 0);
    CHECK_EQ(m.read16(0x708000), static_cast<std::uint16_t>(0x1234));

    m.write16(0x618010, 0x00ab);
    CHECK_EQ(m.read16(0x618010), static_cast<std::uint16_t>(0xffab));
}

TEST("CPS-2: 93C46 EEPROM write and read back over the serial lines") {
    cps1::Cps1Machine m(sfa3(), cps2Regions());
    auto line = [&](bool cs, bool clk, bool di) {
        m.write16(0x804040, static_cast<std::uint16_t>((cs ? 0x4000 : 0) | (clk ? 0x2000 : 0) | (di ? 0x1000 : 0) | 0x08), 0xffff);
    };
    auto bit = [&](bool di) { line(true, false, di); line(true, true, di); };
    auto bits = [&](std::uint32_t v, int n) { for (int i = n - 1; i >= 0; --i) bit((v >> i) & 1); };
    auto deselect = [&] { line(false, false, false); };
    bits(0b100110000, 9); deselect();                     // EWEN
    bits(0b101000101, 9); bits(0xbeef, 16); deselect();   // WRITE word 5
    bits(0b110000101, 9);                                 // READ word 5
    std::uint16_t v = 0;
    for (int i = 0; i < 16; ++i) {
        line(true, false, false);
        line(true, true, false);
        v = static_cast<std::uint16_t>((v << 1) | (m.read16(0x804020) & 1));
    }
    deselect();
    CHECK_EQ(v, static_cast<std::uint16_t>(0xbeef));
    const auto nv = m.nvram();
    CHECK_EQ(nv.size(), static_cast<size_t>(128));
    CHECK_EQ(nv[10], 0xbe);
}

TEST("CPS-2: sprites use the object list, palette and per-sprite priority over tiles") {
    auto r = cps2Regions();
    // Every graphics byte pattern gives pen 5 (planes 0 and 2) everywhere, so
    // the bank shuffle and the tile code do not matter for the test.
    for (size_t i = 0; i < r["gfx"].size(); i += 4) {
        r["gfx"][i] = 0xff;
        r["gfx"][i + 2] = 0xff;
    }
    cps1::Cps1Machine m(sfa3(), std::move(r));
    m.cpsB()[0x30 / 2] = 0x3f;
    m.write16(0x800100 + 0x0a, 0x9140);
    m.write16(0x914000 + (3 * 16 + 5) * 2, 0xff00);          // sprite colour 3: red
    m.write16(0x914000 + ((0x20 + 1) * 16 + 5) * 2, 0xf0f0); // scroll1 colour 1: green
    m.write16(0x800100 + 0x0a, 0x9140);
    // Layer order: scroll1 below the sprites (sprites drawn last on CPS-2).
    m.write16(0x800166, static_cast<std::uint16_t>((1 << 6) | (2 << 8) | (3 << 10) | (0 << 12) | 0x02));
    m.write16(0x800100 + 0x02, 0x9000);                       // scroll1 base
    for (int t = 0; t < 0x1000; ++t) { m.write16(0x900000 + t * 4, 0x20); m.write16(0x900002 + t * 4, 1); }
    m.cps2Output()[2] = 0x0000;  // all layer priorities 0
    auto& obj = m.objram(0);
    obj[0] = static_cast<std::uint16_t>((1 << 13) | 100);  // x, priority 1
    obj[1] = 50;
    obj[2] = 0x10;
    obj[3] = 3;
    obj[4] = static_cast<std::uint16_t>((0 << 13) | 200);  // priority 0: never shown
    obj[5] = 50;
    obj[6] = 0x10;
    obj[7] = 3;
    obj[9] = 0x8000;  // end of list
    m.latchCps2Objects();
    m.renderFrame();
    const std::uint32_t* f = m.frame();
    CHECK_EQ(f[(50) * 384 + 100], 0x00ff0000u);       // sprite in front of the tiles
    CHECK_EQ(f[(50) * 384 + 200], 0x0000ff00u);       // priority-0 sprite hidden: tile shows
    CHECK_EQ(f[(50) * 384 + 300], 0x0000ff00u);
}

TEST("CPS-2: QSound plays a sample voice written by the Z80, at about 24 kHz") {
    auto r = cps2Regions();
    auto& z = r["audiocpu"];
    std::vector<std::uint8_t> p = {0x31, 0x00, 0xff, 0x3a, 0x07, 0xd0, 0xe6, 0x80, 0x28, 0xf9};
    auto reg = [&](int rg, int v) {
        for (std::uint8_t b : {std::uint8_t(0x3e), std::uint8_t(v >> 8), std::uint8_t(0x32), std::uint8_t(0x00), std::uint8_t(0xd0),
                               std::uint8_t(0x3e), std::uint8_t(v & 0xff), std::uint8_t(0x32), std::uint8_t(0x01), std::uint8_t(0xd0),
                               std::uint8_t(0x3e), std::uint8_t(rg), std::uint8_t(0x32), std::uint8_t(0x02), std::uint8_t(0xd0)})
            p.push_back(b);
    };
    reg(0x78, 0x0000);  // bank for voice 0
    reg(0x01, 0x0000);  // start address
    reg(0x02, 0x1000);  // rate 1.0
    reg(0x04, 0x0100);  // loop length
    reg(0x05, 0x0200);  // end address
    reg(0x06, 0x3fff);  // volume
    reg(0x80, 0x0120);  // pan centre
    p.push_back(0x76);
    std::copy(p.begin(), p.end(), z.begin());
    auto& q = r["qsound"];
    for (size_t i = 0; i < 0x400; ++i) q[i] = (i / 8) % 2 ? 0x40 : 0xc0;

    cps1::Cps1Machine m(sfa3(), std::move(r));
    std::vector<std::int16_t> audio;
    for (int f = 0; f < 10; ++f) {
        m.runFrame();
        m.takeAudio(audio);
    }
    CHECK(std::abs(m.audioSampleRate() - 24038) < 2);
    CHECK(audio.size() > 2 * 3900 && audio.size() < 2 * 4200);  // 10 frames at 59.6 Hz
    int peak = 0;
    for (size_t i = audio.size() / 2; i < audio.size(); ++i) peak = std::max(peak, std::abs(static_cast<int>(audio[i])));
    CHECK(peak > 1000);
}

TEST("CPS-2: decryption is applied only inside the key's range and only to program fetches") {
    std::vector<std::uint16_t> rom(0x20000), dec(0x20000);
    for (size_t i = 0; i < rom.size(); ++i) rom[i] = static_cast<std::uint16_t>(i * 2654435761u >> 7);
    const std::uint32_t key[2] = {0x12345678, 0x9abcdef0};
    cps1::cps2crypt::decrypt(rom.data(), dec.data(), static_cast<int>(rom.size() * 2), key, 0, 0x10000 / 2);
    int changed = 0;
    for (size_t i = 0; i < 0x8000; ++i) changed += dec[i] != rom[i];
    CHECK(changed > 0x7000);
    bool outside = true;
    for (size_t i = 0x8001; i < rom.size(); ++i) outside = outside && dec[i] == rom[i];
    CHECK(outside);
    // Different keys, different opcodes
    std::vector<std::uint16_t> dec2(0x20000);
    const std::uint32_t key2[2] = {0x12345679, 0x9abcdef0};
    cps1::cps2crypt::decrypt(rom.data(), dec2.data(), static_cast<int>(rom.size() * 2), key2, 0, 0x10000 / 2);
    CHECK(dec2 != dec);
}
