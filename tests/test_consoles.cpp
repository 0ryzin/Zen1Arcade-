// SPDX-License-Identifier: GPL-3.0-or-later
//
// Console modules: file recognition, names, and each core running a tiny
// cartridge made here (an endless loop; no third-party code or data).
#include "consoles.hpp"
#include "zen1/emu/zip.hpp"
#include "zen1_test.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace zen1;

namespace {

using Bytes = std::vector<std::uint8_t>;

void put(Bytes& b, size_t at, std::initializer_list<int> v) {
    for (int x : v) b[at++] = static_cast<std::uint8_t>(x);
}
void putText(Bytes& b, size_t at, const char* s) {
    while (*s) b[at++] = static_cast<std::uint8_t>(*s++);
}

Bytes nesRom() {  // NROM, 16 KB PRG + 8 KB CHR: JMP $C000
    Bytes b(16 + 16384 + 8192, 0);
    putText(b, 0, "NES\x1a");
    b[4] = 1;
    b[5] = 1;
    put(b, 16, {0x4C, 0x00, 0xC0});
    for (size_t v : {0x3FFAu, 0x3FFCu, 0x3FFEu}) put(b, 16 + v, {0x00, 0xC0});
    return b;
}
Bytes gbRom(bool colour) {  // 32 KB, no mapper: JR $
    Bytes b(0x8000, 0);
    put(b, 0x100, {0x00, 0xC3, 0x50, 0x01});
    put(b, 0x150, {0x18, 0xFE});
    putText(b, 0x134, "ZEN1");
    b[0x143] = colour ? 0x80 : 0x00;
    int x = 0;
    for (int i = 0x134; i < 0x14D; ++i) x = (x - b[static_cast<size_t>(i)] - 1) & 0xFF;
    b[0x14D] = static_cast<std::uint8_t>(x);
    return b;
}
Bytes smsRom(bool gameGear) {  // 32 KB: DI; JR $
    Bytes b(0x8000, 0);
    put(b, 0, {0xF3, 0x18, 0xFE});
    putText(b, 0x7FF0, "TMR SEGA");
    b[0x7FFF] = gameGear ? 0x7C : 0x4C;
    return b;
}
Bytes pceRom() {  // 8 KB bank 0 at $E000 on reset: BRA $
    Bytes b(0x8000, 0);
    put(b, 0, {0x80, 0xFE});
    put(b, 0x1FFE, {0x00, 0xE0});
    return b;
}
Bytes mdRom() {  // 64 KB: SP $FFFE00, PC $200: BRA.S *
    Bytes b(0x10000, 0);
    put(b, 0, {0x00, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x02, 0x00});
    putText(b, 0x100, "SEGA MEGA DRIVE ");
    putText(b, 0x1F0, "JUE");
    put(b, 0x200, {0x60, 0xFE});
    return b;
}
Bytes snesRom() {  // 32 KB LoROM: BRA $ at $8000
    Bytes b(0x8000, 0);
    put(b, 0, {0x80, 0xFE});
    putText(b, 0x7FC0, "ZEN1 TEST            ");
    b[0x7FD5] = 0x20;
    b[0x7FD7] = 0x08;
    b[0x7FD9] = 0x01;
    put(b, 0x7FDC, {0xFF, 0xFF, 0x00, 0x00});
    put(b, 0x7FFC, {0x00, 0x80});
    put(b, 0x7FEA, {0x00, 0x80});
    return b;
}

fs::path tempDir() {
    const fs::path d = fs::temp_directory_path() / ("zen1-consoles-" + std::to_string(::getpid()));
    fs::create_directories(d);
    return d;
}
void writeFile(const fs::path& p, const Bytes& b) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}
// A one-entry stored ZIP.
void writeZip(const fs::path& p, const std::string& name, const Bytes& d) {
    Bytes z;
    auto u16 = [&](unsigned v) { z.push_back(v & 0xFF); z.push_back((v >> 8) & 0xFF); };
    auto u32 = [&](std::uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); };
    const std::uint32_t crc = emu::crc32(d.data(), d.size());
    u32(0x04034b50); u16(20); u16(0); u16(0); u16(0); u16(0); u32(crc);
    u32(static_cast<std::uint32_t>(d.size())); u32(static_cast<std::uint32_t>(d.size()));
    u16(static_cast<unsigned>(name.size())); u16(0);
    z.insert(z.end(), name.begin(), name.end());
    z.insert(z.end(), d.begin(), d.end());
    const auto cd = static_cast<std::uint32_t>(z.size());
    u32(0x02014b50); u16(20); u16(20); u16(0); u16(0); u16(0); u16(0); u32(crc);
    u32(static_cast<std::uint32_t>(d.size())); u32(static_cast<std::uint32_t>(d.size()));
    u16(static_cast<unsigned>(name.size())); u16(0); u16(0); u16(0); u16(0); u32(0); u32(0);
    z.insert(z.end(), name.begin(), name.end());
    const auto cdSize = static_cast<std::uint32_t>(z.size()) - cd;
    u32(0x06054b50); u16(0); u16(0); u16(1); u16(1); u32(cdSize); u32(cd); u16(0);
    writeFile(p, z);
}

// Runs a cartridge for a second; checks the picture size and that sound
// comes at the machine's own rate.
void runs(const fs::path& file, const std::string& system) {
    const consoles::System* sys = consoles::systemForFile(file);
    if (!sys) sys = consoles::systemInZip(file);
    CHECK(sys != nullptr);
    if (!sys) return;
    std::string err;
    const std::string set = consoles::setIdFor(*sys, file);
    consoles::registerRom(set, file);
    auto m = consoles::create(set, &err);
    if (!m) std::printf("    %s: %s\n", file.filename().c_str(), err.c_str());
    CHECK(m != nullptr);
    if (!m) return;
    const auto v = m->video();
    CHECK(v.width > 0 && v.height > 0);
    std::vector<std::int16_t> audio;
    const int frames = 60;
    for (int i = 0; i < frames; ++i) {
        m->runFrame();
        m->takeAudio(audio);
    }
    CHECK(m->frame() != nullptr);
    const double perFrame = static_cast<double>(audio.size()) / 2 / frames;
    const double expected = m->audioSampleRate() / v.refreshHz;
    if (std::abs(perFrame - expected) > expected * 0.1)
        std::printf("    %s: %.1f samples a frame, expected %.1f\n", system.c_str(), perFrame, expected);
    CHECK(std::abs(perFrame - expected) <= expected * 0.1);
}

}  // namespace

TEST("Consoles: systems by extension, .bin only in a Mega Drive folder, titles and ids") {
    CHECK_EQ(consoles::systemForFile("x/Super Game (USA).sfc")->id, std::string("snes"));
    CHECK_EQ(consoles::systemForFile("x/a.SMC")->id, std::string("snes"));
    CHECK_EQ(consoles::systemForFile("x/a.gg")->id, std::string("gg"));
    CHECK_EQ(consoles::systemForFile("x/a.pce")->id, std::string("pce"));
    CHECK(consoles::systemForFile("Games/misc/a.bin") == nullptr);
    CHECK_EQ(consoles::systemForFile("Games/Mega Drive/a.bin")->id, std::string("md"));
    CHECK_EQ(consoles::systemForFile("Games/Genesis/a.bin")->id, std::string("md"));
    CHECK(consoles::systemForFile("x/a.gba")->implemented);
    CHECK_EQ(consoles::systemForFile("x/a.a78")->id, std::string("a7800"));
    CHECK_EQ(consoles::systemForFile("x/a.z64")->id, std::string("n64"));
    CHECK_EQ(consoles::systemForFile("Games/PC Engine CD/Game (Japan).cue")->id, std::string("pcecd"));
    CHECK(consoles::systemForFile("Games/misc/Game.cue") == nullptr);
    CHECK(consoles::systemForFile("Games/Mega CD/Game (Track 1).bin") == nullptr);  // a CD track, not a cartridge
    CHECK(consoles::systemForFile("x/a.zip") == nullptr);
    CHECK_EQ(consoles::titleFromFileName("Legend of Zelda, The - A Link to the Past (USA) [!]"),
             std::string("The Legend of Zelda - A Link to the Past"));
    CHECK_EQ(consoles::titleFromFileName("Super_Tux (World) (Homebrew)"), std::string("Super Tux"));
    CHECK_EQ(consoles::setIdFor(*consoles::systemById("nes"), "Games/Super Tux (World).nes"),
             std::string("nes-super-tux-world"));
}

TEST("Consoles: a zip holding one cartridge; Game Boy colour from the header; PC Engine copier header") {
    const fs::path d = tempDir();
    writeZip(d / "zipped.zip", "Zen1 (World).nes", nesRom());
    CHECK_EQ(consoles::systemInZip(d / "zipped.zip")->id, std::string("nes"));
    const consoles::System* sys = nullptr;
    auto img = consoles::readRom(d / "zipped.zip", &sys, nullptr);
    CHECK(img.has_value());
    CHECK_EQ(sys->id, std::string("nes"));
    CHECK_EQ(img->fileName, std::string("Zen1 (World).nes"));
    writeFile(d / "colour.gb", gbRom(true));
    CHECK(consoles::readRom(d / "colour.gb", &sys, nullptr).has_value());
    CHECK_EQ(sys->id, std::string("gbc"));
    Bytes headered(512, 0);
    const Bytes p = pceRom();
    headered.insert(headered.end(), p.begin(), p.end());
    writeFile(d / "headered.pce", headered);
    img = consoles::readRom(d / "headered.pce", &sys, nullptr);
    CHECK(img && img->data.size() == p.size());
    fs::remove_all(d);
}

TEST("Consoles: every implemented core runs a cartridge, with sound at its own rate") {
    const fs::path d = tempDir();
    writeFile(d / "t.nes", nesRom());
    writeFile(d / "t.gb", gbRom(false));
    writeFile(d / "t.gbc", gbRom(true));
    writeFile(d / "t.sms", smsRom(false));
    writeFile(d / "t.gg", smsRom(true));
    writeFile(d / "t.pce", pceRom());
    writeFile(d / "t.md", mdRom());
    writeFile(d / "t.sfc", snesRom());
    writeZip(d / "zipped-md.zip", "t.gen", mdRom());
    for (const char* f : {"t.nes", "t.gb", "t.gbc", "t.sms", "t.gg", "t.pce", "t.md", "t.sfc", "zipped-md.zip"})
        runs(d / f, f);
    fs::remove_all(d);
}
