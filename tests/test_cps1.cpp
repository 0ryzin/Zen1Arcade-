// SPDX-License-Identifier: GPL-3.0-or-later
// CPS1 machine tests with hand-encoded 68000 code and synthetic graphics.
// No commercial ROM data is used.
#include "cps1.hpp"
#include "zen1/emu/roms.hpp"
#include "zen1/emu/zip.hpp"
#include "zen1_test.hpp"

#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace zen1;
namespace fs = std::filesystem;

namespace {

emu::RegionMap blankRegions() {
    emu::RegionMap r;
    r["maincpu"].assign(0x400000, 0);
    r["gfx"].assign(0x600000, 0);
    r["audiocpu"].assign(0x18000, 0);
    r["oki"].assign(0x40000, 0);
    r["audiocpu"][0] = 0x76;  // Z80: HALT
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

}  // namespace

TEST("CPS1: 68000 boots, takes one vblank interrupt per frame, RAM and IRQ ack work") {
    auto regions = blankRegions();
    auto& rom = regions["maincpu"];
    put32(rom, 0x000000, 0x00fffff0);  // initial stack
    put32(rom, 0x000004, 0x00000400);  // reset PC
    put32(rom, 0x000068, 0x00000500);  // level 2 autovector -> handler
    // 0x400: MOVE.W #$2000,SR ; BRA.S *
    put16(rom, 0x400, 0x46fc); put16(rom, 0x402, 0x2000);
    put16(rom, 0x404, 0x60fe);
    // 0x500: ADDQ.W #1,($FF8000).L ; MOVE.W #$1234,($800180).L (sound latch) ; RTE
    put16(rom, 0x500, 0x5279); put32(rom, 0x502, 0x00ff8000);
    put16(rom, 0x506, 0x33fc); put16(rom, 0x508, 0x1234); put32(rom, 0x50a, 0x00800180);
    put16(rom, 0x50e, 0x4e73);

    cps1::Cps1Machine m(*cps1::findGameDef("sf2"), std::move(regions));
    for (int f = 0; f < 10; ++f) m.runFrame();
    CHECK_EQ(m.read16(0xff8000), static_cast<std::uint16_t>(10));
    CHECK_EQ(m.z80Read(0xf008), static_cast<std::uint8_t>(0x34));  // low byte reached the sound latch
    CHECK(!m.irqPending());
}

TEST("CPS1: input ports read active-low with kicks on the CPS-B extra port") {
    cps1::Cps1Machine m(*cps1::findGameDef("sf2"), blankRegions());
    emu::CabinetInput in;
    in.player[0].right = true;
    in.player[0].button[0] = true;  // light punch
    in.player[0].button[3] = true;  // light kick
    in.player[1].start = true;
    m.setInput(in);
    CHECK_EQ(m.read16(0x800000), static_cast<std::uint16_t>(0xffff & ~0x0011));
    CHECK_EQ(m.read16(0x800018) >> 8, 0xff & ~0x20);                       // P2 start
    CHECK_EQ(m.read16(0x800176) & 0xff, 0xff & ~0x01);                     // P1 light kick
    CHECK_EQ(m.read16(0x800172), static_cast<std::uint16_t>(0x0401));      // CPS-B-11 id
    CHECK_EQ(m.read16(0x80001c) >> 8, 0xfc);                               // DIP B default
}

TEST("CPS1: a sprite is drawn at the right place with the right palette colour") {
    auto regions = blankRegions();
    // Sprite tile code 1 (16x16, 128 bytes): every pixel pen 5 (planes 0 and 2).
    auto& gfx = regions["gfx"];
    for (int row = 0; row < 16; ++row)
        for (int half = 0; half < 2; ++half) {
            const size_t o = 128 + static_cast<size_t>(row) * 8 + static_cast<size_t>(half) * 4;
            gfx[o] = 0xff; gfx[o + 1] = 0x00; gfx[o + 2] = 0xff; gfx[o + 3] = 0x00;
        }
    cps1::Cps1Machine m(*cps1::findGameDef("sf2"), std::move(regions));

    // Palette page 0, colour 3 pen 5 = full-brightness red.
    m.cpsB()[0x30 / 2] = 0x3f;                       // palette control: all pages
    m.write16(0x800100 + 0x0a, 0x9140);              // palette base -> builds palette
    m.write16(0x914000 + (3 * 16 + 5) * 2, 0xff00);  // bright 15, red 15
    m.write16(0x800100 + 0x0a, 0x9140);              // rebuild after writing the colour
    // Sprite list at the object base (0x9200 -> 0x920000).
    m.write16(0x920000, 64 + 10);   // x (raster coordinates)
    m.write16(0x920002, 16 + 20);   // y
    m.write16(0x920004, 1);         // tile
    m.write16(0x920006, 3);         // colour 3, no flip, single tile
    m.write16(0x92000e, 0xff00);    // end of list
    m.latchObjects();
    m.renderFrame();

    const std::uint32_t* f = m.frame();
    CHECK_EQ(f[20 * 384 + 10], 0x00ff0000u);        // top-left of the sprite
    CHECK_EQ(f[35 * 384 + 25], 0x00ff0000u);        // bottom-right
    CHECK(f[19 * 384 + 10] != 0x00ff0000u);         // just above: not sprite
    CHECK(f[20 * 384 + 26] != 0x00ff0000u);         // just right: not sprite
}

TEST("CPS1: game list holds the four implemented games with sensible data") {
    CHECK(emu::findGame("sf2") != nullptr);
    CHECK(emu::findGame("sf2ce") != nullptr);
    CHECK_EQ(emu::findGame("sf2ce")->parent, std::string("sf2"));
    CHECK_EQ(emu::findGame("1941")->rotation, 270);
    CHECK(emu::findGame("pacman") == nullptr);
    std::string err;
    CHECK(emu::createMachine("pacman", "/nonexistent", &err) == nullptr);
    CHECK(err.find("not supported") != std::string::npos);
}

namespace {
// Writes a ZIP with stored (uncompressed) entries.
void writeZip(const fs::path& p, const std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& files) {
    std::vector<std::uint8_t> out, cd;
    auto le16 = [](std::vector<std::uint8_t>& v, std::uint32_t x) { v.push_back(x & 0xff); v.push_back((x >> 8) & 0xff); };
    auto le32 = [&](std::vector<std::uint8_t>& v, std::uint32_t x) { le16(v, x & 0xffff); le16(v, x >> 16); };
    for (const auto& [name, data] : files) {
        const std::uint32_t crc = emu::crc32(data.data(), data.size());
        const auto off = static_cast<std::uint32_t>(out.size());
        le32(out, 0x04034b50); le16(out, 20); le16(out, 0); le16(out, 0); le16(out, 0); le16(out, 0);
        le32(out, crc); le32(out, static_cast<std::uint32_t>(data.size())); le32(out, static_cast<std::uint32_t>(data.size()));
        le16(out, static_cast<std::uint32_t>(name.size())); le16(out, 0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());
        le32(cd, 0x02014b50); le16(cd, 20); le16(cd, 20); le16(cd, 0); le16(cd, 0); le16(cd, 0); le16(cd, 0);
        le32(cd, crc); le32(cd, static_cast<std::uint32_t>(data.size())); le32(cd, static_cast<std::uint32_t>(data.size()));
        le16(cd, static_cast<std::uint32_t>(name.size())); le16(cd, 0); le16(cd, 0); le16(cd, 0); le16(cd, 0);
        le32(cd, 0); le32(cd, off);
        cd.insert(cd.end(), name.begin(), name.end());
    }
    const auto cdOff = static_cast<std::uint32_t>(out.size());
    out.insert(out.end(), cd.begin(), cd.end());
    le32(out, 0x06054b50); le16(out, 0); le16(out, 0);
    le16(out, static_cast<std::uint32_t>(files.size())); le16(out, static_cast<std::uint32_t>(files.size()));
    le32(out, static_cast<std::uint32_t>(cd.size())); le32(out, cdOff); le16(out, 0);
    std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
}
}  // namespace

TEST("ROM loader: byte-interleave, word-swap, 64-bit word and continue layouts; parent zip fallback") {
    const fs::path dir = fs::temp_directory_path() / ("zen1-roms-" + std::to_string(::getpid()));
    fs::remove_all(dir);
    fs::create_directories(dir / "Capcom");
    const std::vector<std::uint8_t> even = {1, 2}, odd = {3, 4}, sw = {0xaa, 0xbb, 0xcc, 0xdd},
                                    w64 = {5, 6, 7, 8}, z80 = {9, 10, 11, 12};
    writeZip(dir / "Capcom" / "game.zip", {{"even.bin", even}, {"odd.bin", odd}, {"sw.bin", sw}});
    writeZip(dir / "PARENT.ZIP", {{"gfx.bin", w64}, {"snd.bin", z80}});

    using emu::RomLoad;
    const std::vector<emu::RomFile> files = {
        {"cpu", "even.bin", emu::crc32(even.data(), 2), 0, 2, RomLoad::Byte16},
        {"cpu", "odd.bin", emu::crc32(odd.data(), 2), 1, 2, RomLoad::Byte16},
        {"cpu", "sw.bin", emu::crc32(sw.data(), 4), 4, 4, RomLoad::WordSwap16},
        {"gfx", "gfx.bin", emu::crc32(w64.data(), 4), 2, 4, RomLoad::Word64},
        {"snd", "snd.bin", emu::crc32(z80.data(), 4), 0, 2, RomLoad::Plain},
        {"snd", nullptr, 0, 8, 2, RomLoad::Continue},
    };
    const auto r = emu::loadRomSet(dir, "game", "parent", {{"cpu", 8}, {"gfx", 16}, {"snd", 10}}, files);
    CHECK(r.ok);
    CHECK((r.regions.at("cpu") == std::vector<std::uint8_t>{1, 3, 2, 4, 0xbb, 0xaa, 0xdd, 0xcc}));
    CHECK((r.regions.at("gfx") == std::vector<std::uint8_t>{0, 0, 5, 6, 0, 0, 0, 0, 0, 0, 7, 8, 0, 0, 0, 0}));
    CHECK((r.regions.at("snd") == std::vector<std::uint8_t>{9, 10, 0, 0, 0, 0, 0, 0, 11, 12}));

    auto bad = files;
    bad[0].crc ^= 1;  // a different ROM version
    const auto r2 = emu::loadRomSet(dir, "game", "parent", {{"cpu", 8}, {"gfx", 16}, {"snd", 10}}, bad);
    CHECK(!r2.ok);
    CHECK_EQ(r2.missing.size(), static_cast<size_t>(1));
    CHECK(emu::checkRomSet(dir, "game", "parent", files).empty());
    fs::remove_all(dir);
}
