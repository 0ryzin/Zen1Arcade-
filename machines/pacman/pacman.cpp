// SPDX-License-Identifier: GPL-3.0-or-later
#include "pacman.hpp"

extern "C" {
#include "z80.h"
}

#include <algorithm>
#include <cstring>

namespace zen1::pacman {

// ----------------------------------------------------------------- WSG

void Wsg::reset() {
    regs_.fill(0);
    acc_.fill(0);
    enabled_ = false;
}

void Wsg::write(int reg, std::uint8_t v) { regs_[static_cast<size_t>(reg & 0x1f)] = v & 0x0f; }

int Wsg::sample() {
    if (!enabled_ || !prom_) return 0;
    int out = 0;
    for (int ch = 0; ch < 3; ++ch) {
        // Voice 0: frequency nibbles at 0x10-0x14; voices 1/2 at 0x16-0x19 / 0x1b-0x1e (low nibble 0).
        std::uint32_t f = 0;
        const int base = 0x10 + ch * 5;
        if (ch == 0) f = regs_[0x10];
        for (int k = 1; k <= 4; ++k) f |= static_cast<std::uint32_t>(regs_[static_cast<size_t>(base + k)]) << (4 * k);
        const int vol = regs_[static_cast<size_t>(0x15 + ch * 5)];
        const int wave = regs_[static_cast<size_t>(0x05 + ch * 5)] & 7;
        acc_[static_cast<size_t>(ch)] = (acc_[static_cast<size_t>(ch)] + f) & 0xfffff;
        if (vol == 0 || f == 0) continue;
        const int idx = static_cast<int>(acc_[static_cast<size_t>(ch)] >> 15) & 0x1f;
        const int s = (prom_[wave * 32 + idx] & 0x0f) - 8;
        out += s * vol;
    }
    return out;  // -360 .. 315
}

// ----------------------------------------------------------------- machine

namespace {
constexpr double kCpuClock = 3'072'000.0;
constexpr int kLines = 264;
constexpr double kCyclesPerLine = 192.0;  // 384 pixel clocks / 2
constexpr int kWsgRate = 96'000;
}  // namespace

PacmanMachine::PacmanMachine(const emu::GameInfo& info, Variant v, emu::RegionMap regions, std::uint8_t dsw1)
    : info_(info), variant_(v), regions_(std::move(regions)), dsw1_(dsw1) {
    auto& main = regions_["maincpu"];
    if (variant_ == Variant::MsPacman) {
        // Ms. Pac-Man daughterboard: decrypt the aux ROMs into a second bank
        // and apply the 40 patches (MAME init_mspacman).
        rom_.assign(0x20000, 0);
        std::copy(main.begin(), main.begin() + std::min<size_t>(main.size(), 0x10000), rom_.begin());
        auto bs8 = [](std::uint8_t x) {  // bitswap<8>(x, 0,4,5,7,6,3,2,1)
            const int order[8] = {0, 4, 5, 7, 6, 3, 2, 1};
            std::uint8_t r = 0;
            for (int i = 0; i < 8; ++i) r = static_cast<std::uint8_t>(r | (((x >> order[i]) & 1) << (7 - i)));
            return r;
        };
        auto bsN = [](int x, std::initializer_list<int> order) {
            int r = 0, n = static_cast<int>(order.size()), i = 0;
            for (int b : order) r |= ((x >> b) & 1) << (n - 1 - i++);
            return r;
        };
        std::uint8_t* R = rom_.data();
        std::uint8_t* D = rom_.data() + 0x10000;
        for (int i = 0; i < 0x1000; ++i) {
            D[0x0000 + i] = R[0x0000 + i];
            D[0x1000 + i] = R[0x1000 + i];
            D[0x2000 + i] = R[0x2000 + i];
            D[0x3000 + i] = bs8(R[0xb000 + bsN(i, {11, 3, 7, 9, 10, 8, 6, 5, 4, 2, 1, 0})]);
        }
        for (int i = 0; i < 0x800; ++i) {
            D[0x8000 + i] = bs8(R[0x8000 + bsN(i, {8, 7, 5, 9, 10, 6, 3, 4, 2, 1, 0})]);
            D[0x8800 + i] = bs8(R[0x9800 + bsN(i, {3, 7, 9, 10, 8, 6, 5, 4, 2, 1, 0})]);
            D[0x9000 + i] = bs8(R[0x9000 + bsN(i, {3, 7, 9, 10, 8, 6, 5, 4, 2, 1, 0})]);
            D[0x9800 + i] = R[0x1800 + i];
        }
        for (int i = 0; i < 0x1000; ++i) {
            D[0xa000 + i] = R[0x2000 + i];
            D[0xb000 + i] = R[0x3000 + i];
        }
        static const std::uint16_t patches[][2] = {
            {0x0410, 0x8008}, {0x08e0, 0x81d8}, {0x0a30, 0x8118}, {0x0bd0, 0x80d8}, {0x0c20, 0x8120},
            {0x0e58, 0x8168}, {0x0ea8, 0x8198}, {0x1000, 0x8020}, {0x1008, 0x8010}, {0x1288, 0x8098},
            {0x1348, 0x8048}, {0x1688, 0x8088}, {0x16b0, 0x8188}, {0x16d8, 0x80c8}, {0x16f8, 0x81c8},
            {0x19a8, 0x80a8}, {0x19b8, 0x81a8}, {0x2060, 0x8148}, {0x2108, 0x8018}, {0x21a0, 0x81a0},
            {0x2298, 0x80a0}, {0x23e0, 0x80e8}, {0x2418, 0x8000}, {0x2448, 0x8058}, {0x2470, 0x8140},
            {0x2488, 0x8080}, {0x24b0, 0x8180}, {0x24d8, 0x80c0}, {0x24f8, 0x81c0}, {0x2748, 0x8050},
            {0x2780, 0x8090}, {0x27b8, 0x8190}, {0x2800, 0x8028}, {0x2b20, 0x8100}, {0x2b30, 0x8110},
            {0x2bf0, 0x81d0}, {0x2cc0, 0x80d0}, {0x2cd8, 0x80e0}, {0x2cf0, 0x81e0}, {0x2d60, 0x8160}};
        for (const auto& p : patches)
            for (int i = 0; i < 8; ++i) D[p[0] + i] = D[p[1] + i];
        for (int i = 0; i < 0x1000; ++i) {
            R[0x8000 + i] = R[0x0000 + i];
            R[0x9000 + i] = R[0x1000 + i];
            R[0xa000 + i] = R[0x2000 + i];
            R[0xb000 + i] = R[0x3000 + i];
        }
    } else {
        rom_.assign(0x10000, 0);
        std::copy(main.begin(), main.begin() + std::min<size_t>(main.size(), 0x4000), rom_.begin());
    }
    decodeGfx();
    buildPalette();
    frame_.assign(static_cast<size_t>(kWidth * kHeight), 0);
    wsg_.setPROM(regions_["namco"].data());

    cpu_ = std::make_unique<::z80>();
    reset();
}

PacmanMachine::~PacmanMachine() = default;

emu::VideoInfo PacmanMachine::video() const {
    // 288x224 shown at 4:3 on its side (ROT90); 6.144 MHz / 384 / 264.
    return emu::VideoInfo{kWidth, kHeight, 4.0 / 3.0, info_.rotation, 6'144'000.0 / 384.0 / 264.0};
}

void PacmanMachine::reset() {
    z80_init(cpu_.get());
    cpu_->userdata = this;
    cpu_->read_byte = [](void* u, std::uint16_t a) { return static_cast<PacmanMachine*>(u)->read(a); };
    cpu_->write_byte = [](void* u, std::uint16_t a, std::uint8_t v) { static_cast<PacmanMachine*>(u)->write(a, v); };
    cpu_->port_in = [](::z80*, std::uint8_t) -> std::uint8_t { return 0xff; };
    cpu_->port_out = [](::z80* z, std::uint8_t port, std::uint8_t v) {
        if (port == 0) static_cast<PacmanMachine*>(z->userdata)->irqVector_ = v;
    };
    latch_.fill(0);
    wsg_.reset();
    decodeBank_ = true;
    vram_.fill(0);
    cram_.fill(0);
    ram_.fill(0);
    spriteRam_.fill(0);
    spriteRam2_.fill(0);
    audio_.clear();
}

void PacmanMachine::setInput(const emu::CabinetInput& in) {
    // Upright cabinet: both panels drive the one game joystick (players take turns).
    std::uint8_t i0 = 0xff, i1 = 0xff;
    for (const auto& p : in.player) {
        if (p.up) i0 &= ~0x01;
        if (p.left) i0 &= ~0x02;
        if (p.right) i0 &= ~0x04;
        if (p.down) i0 &= ~0x08;
    }
    if (in.player[0].coin) i0 &= ~0x20;
    if (in.player[1].coin) i0 &= ~0x40;
    if (in.service) i0 &= ~0x80;
    if (in.player[0].start) i1 &= ~0x20;
    if (in.player[1].start) i1 &= ~0x40;
    in0_ = i0;
    in1_ = i1;  // bit 7 = upright cabinet, bit 4 = service mode off
}

void PacmanMachine::checkMsPacDecode(std::uint16_t a) {
    if (variant_ != Variant::MsPacman) return;
    if ((a >= 0x3ff8 && a <= 0x3fff)) decodeBank_ = true;
    else if ((a >= 0x0038 && a <= 0x003f) || (a >= 0x03b0 && a <= 0x03b7) || (a >= 0x1600 && a <= 0x1607) ||
             (a >= 0x2120 && a <= 0x2127) || (a >= 0x3ff0 && a <= 0x3ff7) || (a >= 0x8000 && a <= 0x8007) ||
             (a >= 0x97f0 && a <= 0x97f7))
        decodeBank_ = false;
}

std::uint8_t PacmanMachine::read(std::uint16_t a) {
    if ((a & 0x4000) == 0) {  // ROM space 0x0000-0x3fff and 0x8000-0xbfff
        if (variant_ == Variant::MsPacman) {
            // The toggles return the byte of the bank they switch to.
            checkMsPacDecode(a);
            return rom_[(decodeBank_ ? 0x10000u : 0u) + a];
        }
        return rom_[a & 0x3fff];
    }
    const std::uint16_t n = a & 0x5fff;  // A13 and A15 are not decoded
    if (n < 0x4400) return vram_[n & 0x3ff];
    if (n < 0x4800) return cram_[n & 0x3ff];
    if (n < 0x4c00) return 0xbf;  // unconnected
    if (n < 0x4ff0) return ram_[n - 0x4c00];
    if (n < 0x5000) return spriteRam_[n & 0x0f];
    switch (n & 0x50c0) {
    case 0x5000: return static_cast<std::uint8_t>((in0_ & ~0x10) | 0x10);  // rack test off
    case 0x5040: return in1_;
    case 0x5080: return dsw1_;
    default: return 0x00;  // DSW2: unused
    }
}

void PacmanMachine::write(std::uint16_t a, std::uint8_t v) {
    if ((a & 0x4000) == 0) {
        checkMsPacDecode(a);
        return;
    }
    const std::uint16_t n = a & 0x5fff;
    if (n < 0x4400) { vram_[n & 0x3ff] = v; return; }
    if (n < 0x4800) { cram_[n & 0x3ff] = v; return; }
    if (n < 0x4c00) return;
    if (n < 0x4ff0) { ram_[n - 0x4c00] = v; return; }
    if (n < 0x5000) { spriteRam_[n & 0x0f] = v; return; }
    const std::uint16_t io = n & 0x50ff;
    if ((io & 0xffc0) == 0x5000) {  // 74LS259 latch at 0x5000-0x5007 (mirrored)
        const int bit = io & 7;
        latch_[static_cast<size_t>(bit)] = v & 1;
        if (bit == 0 && !(v & 1)) cpu_->int_pending = false;  // IRQ mask off clears the line
        if (bit == 1) wsg_.enable(v & 1);
        return;
    }
    if (io >= 0x5040 && io < 0x5060) { wsg_.write(io - 0x5040, v); return; }
    if (io >= 0x5060 && io < 0x5070) { spriteRam2_[io & 0x0f] = v; return; }
    // 0x50c0 watchdog, 0x5070/0x5080: ignored
}

void PacmanMachine::decodeGfx() {
    const auto& g = regions_["gfx1"];
    auto bit = [&](int base, int off) { return (g[static_cast<size_t>(base + off / 8)] >> (7 - off % 8)) & 1; };
    // Pixel value: plane 0 (MSB) at bit offset +0, plane 1 at +4 (MAME planes {0,4}).
    tiles_.assign(256 * 64, 0);
    static const int txo[8] = {64, 65, 66, 67, 0, 1, 2, 3};
    for (int t = 0; t < 256; ++t)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const int off = txo[x] + y * 8;
                tiles_[static_cast<size_t>(t * 64 + y * 8 + x)] =
                    static_cast<std::uint8_t>((bit(t * 16, off) << 1) | bit(t * 16, off + 4));
            }
    sprites_.assign(64 * 256, 0);
    static const int sxo[16] = {64, 65, 66, 67, 128, 129, 130, 131, 192, 193, 194, 195, 0, 1, 2, 3};
    static const int syo[16] = {0, 8, 16, 24, 32, 40, 48, 56, 256, 264, 272, 280, 288, 296, 304, 312};
    for (int s = 0; s < 64; ++s)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const int off = sxo[x] + syo[y];
                const int base = 0x1000 + s * 64;
                sprites_[static_cast<size_t>(s * 256 + y * 16 + x)] =
                    static_cast<std::uint8_t>((bit(base, off) << 1) | bit(base, off + 4));
            }
}

void PacmanMachine::buildPalette() {
    const auto& p = regions_["proms"];
    for (int i = 0; i < 32; ++i) {
        const std::uint8_t c = p[static_cast<size_t>(i)];
        // Resistor weights 1k/470/220 ohm (red, green) and 470/220 (blue).
        const int r = 0x21 * (c & 1) + 0x47 * ((c >> 1) & 1) + 0x97 * ((c >> 2) & 1);
        const int g = 0x21 * ((c >> 3) & 1) + 0x47 * ((c >> 4) & 1) + 0x97 * ((c >> 5) & 1);
        const int b = 0x51 * ((c >> 6) & 1) + 0xae * ((c >> 7) & 1);
        rgb_[static_cast<size_t>(i)] = static_cast<std::uint32_t>(r << 16 | g << 8 | b);
    }
}

void PacmanMachine::drawSprite(int code, int color, bool fx, bool fy, int sx, int sy) {
    const auto& lut = regions_["proms"];
    for (int y = 0; y < 16; ++y) {
        const int py = sy + y;
        if (py < 0 || py >= kHeight) continue;
        for (int x = 0; x < 16; ++x) {
            const int px = sx + x;
            if (px < 16 || px >= 272) continue;  // sprite clip: columns 2-33
            const int pen = sprites_[static_cast<size_t>(code * 256 + (fy ? 15 - y : y) * 16 + (fx ? 15 - x : x))];
            const int entry = lut[static_cast<size_t>(0x20 + ((color & 0x3f) * 4 + pen) % 256)] & 0x0f;
            if (entry == 0) continue;  // transparent where the colour lookup gives 0
            frame_[static_cast<size_t>(py * kWidth + px)] = rgb_[static_cast<size_t>(entry)];
        }
    }
}

void PacmanMachine::render() {
    const auto& lut = regions_["proms"];
    // Background: 36x28 tiles, MAME pacman_scan_rows mapping.
    for (int row = 0; row < 28; ++row)
        for (int col = 0; col < 36; ++col) {
            const int r = row + 2, c = col - 2;
            const int idx = (c & 0x20) ? r + ((c & 0x1f) << 5) : c + (r << 5);
            const int code = vram_[static_cast<size_t>(idx & 0x3ff)];
            const int color = cram_[static_cast<size_t>(idx & 0x3ff)] & 0x1f;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    const int pen = tiles_[static_cast<size_t>(code * 64 + y * 8 + x)];
                    const int entry = lut[static_cast<size_t>(0x20 + color * 4 + pen)] & 0x0f;
                    frame_[static_cast<size_t>((row * 8 + y) * kWidth + col * 8 + x)] = rgb_[static_cast<size_t>(entry)];
                }
        }
    // Sprites 7..3, then 2..0 shifted by one pixel (MAME xoffsethack), each
    // also drawn 256 pixels to the left for the tunnel wraparound.
    for (int offs = 14; offs >= 0; offs -= 2) {
        const int sx = 272 - spriteRam2_[static_cast<size_t>(offs + 1)];
        const int sy = spriteRam2_[static_cast<size_t>(offs)] - 31 + (offs <= 4 ? 1 : 0);
        const int code = spriteRam_[static_cast<size_t>(offs)] >> 2;
        const bool fx = spriteRam_[static_cast<size_t>(offs)] & 1, fy = spriteRam_[static_cast<size_t>(offs)] & 2;
        const int color = spriteRam_[static_cast<size_t>(offs + 1)] & 0x1f;
        drawSprite(code, color, fx, fy, sx, sy);
        drawSprite(code, color, fx, fy, sx - 256, sy);
    }
}

void PacmanMachine::runFrame() {
    ::z80* z = cpu_.get();
    double target = static_cast<double>(z->cyc);
    const double samplesPerCycle = kWsgRate / kCpuClock;
    for (int line = 0; line < kLines; ++line) {
        if (line == 224) {
            render();
            if (latch_[0]) z80_gen_int(z, irqVector_);  // VBLANK IRQ (held until taken)
        }
        target += kCyclesPerLine;
        while (static_cast<double>(z->cyc) < target) {
            const unsigned long before = z->cyc;
            if (latch_[0] && z->int_pending) z->int_data = irqVector_;
            z80_step(z);
            sampleAcc_ += static_cast<double>(z->cyc - before) * samplesPerCycle;
            while (sampleAcc_ >= 2.0) {  // 96 kHz chip output averaged down to 48 kHz
                sampleAcc_ -= 2.0;
                const int s = wsg_.sample() + wsg_.sample();
                const auto v = static_cast<std::int16_t>(std::clamp(s * 22, -32000, 32000));
                audio_.push_back(v);
                audio_.push_back(v);
            }
        }
    }
    if (z->cyc > 1'000'000'000UL) z->cyc -= 900'000'000UL;
}

void PacmanMachine::takeAudio(std::vector<std::int16_t>& out) {
    out.insert(out.end(), audio_.begin(), audio_.end());
    audio_.clear();
}

// ----------------------------------------------------------------- games

namespace {

struct Def {
    emu::GameInfo info;
    Variant variant;
    std::uint8_t dsw1;
    std::vector<emu::RomFile> roms;
};

using emu::RomLoad;

const std::vector<Def>& defs() {
    static const std::vector<Def> d = [] {
        std::vector<Def> v;
        auto info = [](const char* set, const char* parent, const char* title, const char* year, const char* maker) {
            emu::GameInfo g;
            g.set = set; g.parent = parent; g.title = title; g.year = year; g.manufacturer = maker;
            g.system = "Namco Pac-Man"; g.players = 2; g.buttons = 0; g.rotation = 90;
            g.joystick = "4-way";
            return g;
        };
        const std::vector<emu::RomFile> pacProms = {
            {"proms", "82s123.7f", 0x2fc650bd, 0x0000, 0x0020, RomLoad::Plain},
            {"proms", "82s126.4a", 0x3eb3a8e4, 0x0020, 0x0100, RomLoad::Plain},
            {"namco", "82s126.1m", 0xa9cc86bf, 0x0000, 0x0100, RomLoad::Plain},
        };
        {   // Pac-Man (Midway)
            Def g{info("pacman", "puckman", "Pac-Man", "1980", "Namco (Midway license)"), Variant::Pacman, 0xc9, {
                {"maincpu", "pacman.6e", 0xc1e6ab10, 0x0000, 0x1000, RomLoad::Plain},
                {"maincpu", "pacman.6f", 0x1a6fb2d4, 0x1000, 0x1000, RomLoad::Plain},
                {"maincpu", "pacman.6h", 0xbcdd1beb, 0x2000, 0x1000, RomLoad::Plain},
                {"maincpu", "pacman.6j", 0x817d94e3, 0x3000, 0x1000, RomLoad::Plain},
                {"gfx1", "pacman.5e", 0x0c944964, 0x0000, 0x1000, RomLoad::Plain},
                {"gfx1", "pacman.5f", 0x958fedf9, 0x1000, 0x1000, RomLoad::Plain},
            }};
            g.roms.insert(g.roms.end(), pacProms.begin(), pacProms.end());
            v.push_back(g);
        }
        {   // Puck Man (Japan)
            Def g{info("puckman", "", "Puck Man", "1980", "Namco"), Variant::Pacman, 0xc9, {
                {"maincpu", "pm1_prg1.6e", 0xf36e88ab, 0x0000, 0x0800, RomLoad::Plain},
                {"maincpu", "pm1_prg2.6k", 0x618bd9b3, 0x0800, 0x0800, RomLoad::Plain},
                {"maincpu", "pm1_prg3.6f", 0x7d177853, 0x1000, 0x0800, RomLoad::Plain},
                {"maincpu", "pm1_prg4.6m", 0xd3e8914c, 0x1800, 0x0800, RomLoad::Plain},
                {"maincpu", "pm1_prg5.6h", 0x6bf4f625, 0x2000, 0x0800, RomLoad::Plain},
                {"maincpu", "pm1_prg6.6n", 0xa948ce83, 0x2800, 0x0800, RomLoad::Plain},
                {"maincpu", "pm1_prg7.6j", 0xb6289b26, 0x3000, 0x0800, RomLoad::Plain},
                {"maincpu", "pm1_prg8.6p", 0x17a88c13, 0x3800, 0x0800, RomLoad::Plain},
                {"gfx1", "pm1_chg1.5e", 0x2066a0b7, 0x0000, 0x0800, RomLoad::Plain},
                {"gfx1", "pm1_chg2.5h", 0x3591b89d, 0x0800, 0x0800, RomLoad::Plain},
                {"gfx1", "pm1_chg3.5f", 0x9e39323a, 0x1000, 0x0800, RomLoad::Plain},
                {"gfx1", "pm1_chg4.5j", 0x1b1d9096, 0x1800, 0x0800, RomLoad::Plain},
                {"proms", "pm1-1.7f", 0x2fc650bd, 0x0000, 0x0020, RomLoad::Plain},
                {"proms", "pm1-4.4a", 0x3eb3a8e4, 0x0020, 0x0100, RomLoad::Plain},
                {"namco", "pm1-3.1m", 0xa9cc86bf, 0x0000, 0x0100, RomLoad::Plain},
            }};
            v.push_back(g);
        }
        {   // Ms. Pac-Man (Midway)
            Def g{info("mspacman", "", "Ms. Pac-Man", "1981", "Midway / General Computer Corporation"),
                  Variant::MsPacman, 0xc9, {
                {"maincpu", "pacman.6e", 0xc1e6ab10, 0x0000, 0x1000, RomLoad::Plain},
                {"maincpu", "pacman.6f", 0x1a6fb2d4, 0x1000, 0x1000, RomLoad::Plain},
                {"maincpu", "pacman.6h", 0xbcdd1beb, 0x2000, 0x1000, RomLoad::Plain},
                {"maincpu", "pacman.6j", 0x817d94e3, 0x3000, 0x1000, RomLoad::Plain},
                {"maincpu", "u5", 0xf45fbbcd, 0x8000, 0x0800, RomLoad::Plain},
                {"maincpu", "u6", 0xa90e7000, 0x9000, 0x1000, RomLoad::Plain},
                {"maincpu", "u7", 0xc82cd714, 0xb000, 0x1000, RomLoad::Plain},
                {"gfx1", "5e", 0x5c281d01, 0x0000, 0x1000, RomLoad::Plain},
                {"gfx1", "5f", 0x615af909, 0x1000, 0x1000, RomLoad::Plain},
            }};
            g.roms.insert(g.roms.end(), pacProms.begin(), pacProms.end());
            v.push_back(g);
        }
        return v;
    }();
    return d;
}

const Def* findDef(const std::string& set) {
    for (const auto& d : defs())
        if (d.info.set == set) return &d;
    return nullptr;
}

}  // namespace

std::vector<emu::GameInfo> moduleGames() {
    std::vector<emu::GameInfo> v;
    for (const auto& d : defs()) v.push_back(d.info);
    return v;
}

std::vector<emu::RomFile> moduleRomFiles(const std::string& set) {
    const auto* d = findDef(set);
    return d ? d->roms : std::vector<emu::RomFile>{};
}

std::unique_ptr<emu::Machine> moduleCreate(const std::string& set, const std::filesystem::path& gamesDir,
                                           std::string* error) {
    const auto* d = findDef(set);
    if (!d) return nullptr;
    const std::vector<emu::RomRegion> regions = {
        {"maincpu", d->variant == Variant::MsPacman ? 0x10000u : 0x4000u}, {"gfx1", 0x2000}, {"proms", 0x120},
        {"namco", 0x100}};
    auto loaded = emu::loadRomSet(gamesDir, d->info.set, d->info.parent, regions, d->roms);
    if (!loaded.ok) {
        if (error) {
            *error = "ROM set '" + set + "' is incomplete or a different version. Missing or wrong:";
            for (const auto& m : loaded.missing) *error += "\n  " + m;
        }
        return nullptr;
    }
    return std::make_unique<PacmanMachine>(d->info, d->variant, std::move(loaded.regions), d->dsw1);
}

}  // namespace zen1::pacman
