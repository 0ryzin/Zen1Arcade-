// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom 1942 hardware for the Zen1 engine.
//
// Hardware behaviour ported from MAME's 1942.cpp / 1942_v.cpp (licence
// BSD-3-Clause, copyright-holders: Paul Leaman, Couriersud): memory maps,
// PROM-timed interrupts, graphics layouts, palette PROM decoding, the
// scanline sprite engine and the ROM lists. Rewritten for Zen1; see
// THIRD_PARTY.md.
//
// Board: main Z80 at 3 MHz, sound Z80 at 3 MHz, two AY-3-8910 at 1.5 MHz
// (Zen1 uses ymfm's SSG engine for them; MAME's netlist output filter is
// approximated by one low-pass), 256x224 picture, 59.6 Hz, rotated 270.
#include "c1942.hpp"

#include <cstddef>
#include <ymfm_ssg.h>

extern "C" {
#include <z80.h>
}

#include <algorithm>
#include <cmath>

namespace zen1::c1942 {

namespace {
constexpr double kPixelClock = 6'000'000.0;
constexpr int kHTotal = 384, kVTotal = 262, kFirstLine = 16;
constexpr double kCpuClock = 3'000'000.0;
constexpr double kAyClock = 1'500'000.0;
constexpr int kAudioRate = 46875;  // AY engine (clock/8) / 4

int bit(const std::vector<std::uint8_t>& r, size_t pos) {
    const size_t byte = pos / 8;
    return byte < r.size() ? (r[byte] >> (7 - pos % 8)) & 1 : 0;
}
}  // namespace

struct C1942Machine::Ay : ymfm::ymfm_interface {
    ymfm::ssg_engine engine{*this};
    std::uint8_t address = 0;
    void write(int port, std::uint8_t v) {
        if (port == 0) address = v & 0x0f;
        else engine.write(address, v);
    }
    int step() {
        engine.clock();
        ymfm::ssg_engine::output_data out;
        engine.output(out);
        return out.data[0] + out.data[1] + out.data[2];
    }
};

C1942Machine::C1942Machine(const emu::GameInfo& info, emu::RegionMap regions, std::array<std::uint8_t, 2> dips)
    : info_(info), regions_(std::move(regions)), dips_(dips) {
    decodeGfx();
    buildPalette();
    main_ = std::make_unique<::z80>();
    sound_ = std::make_unique<::z80>();
    ay_[0] = std::make_unique<Ay>();
    ay_[1] = std::make_unique<Ay>();
    frame_.assign(static_cast<size_t>(kWidth * kHeight), 0);
    reset();
}

C1942Machine::~C1942Machine() = default;

emu::VideoInfo C1942Machine::video() const {
    return emu::VideoInfo{kWidth, kHeight, 4.0 / 3.0, info_.rotation, kPixelClock / (kHTotal * kVTotal)};
}

int C1942Machine::audioSampleRate() const { return kAudioRate; }

void C1942Machine::decodeGfx() {
    // Characters: 8x8, 2 bits, planes {4, 0}; x {0-3, 8-11}; 16 bits a row.
    const auto& c = regions_["gfx1"];
    const size_t nChars = c.size() / 16;
    chars_.assign(nChars * 64, 0);
    static const int cx[8] = {0, 1, 2, 3, 8, 9, 10, 11};
    for (size_t n = 0; n < nChars; ++n)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const size_t base = n * 128 + static_cast<size_t>(y * 16 + cx[x]);
                chars_[n * 64 + static_cast<size_t>(y * 8 + x)] =
                    static_cast<std::uint8_t>((bit(c, base + 4) << 1) | bit(c, base));
            }
    // Tiles: 16x16, 3 bits, one plane per third of the region.
    const auto& t = regions_["gfx2"];
    const size_t third = t.size() * 8 / 3;
    const size_t nTiles = t.size() / 3 / 32;
    tiles_.assign(nTiles * 256, 0);
    for (size_t n = 0; n < nTiles; ++n)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const size_t base = n * 256 + static_cast<size_t>(y * 8 + (x < 8 ? x : 128 + x - 8));
                tiles_[n * 256 + static_cast<size_t>(y * 16 + x)] = static_cast<std::uint8_t>(
                    (bit(t, base) << 2) | (bit(t, third + base) << 1) | bit(t, 2 * third + base));
            }
    // Sprites: 16x16, 4 bits, planes {half+4, half+0, 4, 0}.
    const auto& s = regions_["gfx3"];
    const size_t half = s.size() * 8 / 2;
    const size_t nSpr = s.size() / 2 / 64;
    static const int sx[16] = {0, 1, 2, 3, 8, 9, 10, 11, 256, 257, 258, 259, 264, 265, 266, 267};
    sprites_.assign(nSpr * 256, 0);
    for (size_t n = 0; n < nSpr; ++n)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const size_t base = n * 512 + static_cast<size_t>(y * 16 + sx[x]);
                sprites_[n * 256 + static_cast<size_t>(y * 16 + x)] = static_cast<std::uint8_t>(
                    (bit(s, half + base + 4) << 3) | (bit(s, half + base) << 2) | (bit(s, base + 4) << 1) | bit(s, base));
            }
}

void C1942Machine::buildPalette() {
    // Three 256x4 PROMs (R, G, B), weights 220/470/1k/2.2k ohm (MAME).
    const auto& p = regions_["palproms"];
    for (int i = 0; i < 256; ++i) {
        auto gun = [&](int n) {
            const int v = static_cast<size_t>(i + n * 256) < p.size() ? p[static_cast<size_t>(i + n * 256)] : 0;
            return 0x0e * (v & 1) + 0x1f * ((v >> 1) & 1) + 0x43 * ((v >> 2) & 1) + 0x8f * ((v >> 3) & 1);
        };
        rgb_[static_cast<size_t>(i)] = static_cast<std::uint32_t>((gun(0) << 16) | (gun(1) << 8) | gun(2));
    }
}

void C1942Machine::reset() {
    ram_.fill(0);
    fgram_.fill(0);
    bgram_.fill(0);
    spriteram_.fill(0);
    soundRam_.fill(0);
    latch_ = {0, 0};
    scroll_ = 0;
    paletteBank_ = 0;
    bank_ = 0;
    soundHeld_ = false;
    for (::z80* z : {main_.get(), sound_.get()}) {
        z80_init(z);
        z->userdata = this;
        z->port_in = [](::z80*, std::uint8_t) -> std::uint8_t { return 0xff; };
        z->port_out = [](::z80*, std::uint8_t, std::uint8_t) {};
    }
    main_->read_byte = [](void* u, std::uint16_t a) { return static_cast<C1942Machine*>(u)->read(a); };
    main_->write_byte = [](void* u, std::uint16_t a, std::uint8_t v) { static_cast<C1942Machine*>(u)->write(a, v); };
    sound_->read_byte = [](void* u, std::uint16_t a) { return static_cast<C1942Machine*>(u)->soundRead(a); };
    sound_->write_byte = [](void* u, std::uint16_t a, std::uint8_t v) { static_cast<C1942Machine*>(u)->soundWrite(a, v); };
    for (auto& a : ay_) a->engine.reset();
    mainTarget_ = soundTarget_ = 0;
    ayAcc_ = 0;
    lp_ = 0;
    audio_.clear();
}

void C1942Machine::setInput(const emu::CabinetInput& in) {
    std::uint8_t sys = 0xff;
    if (in.player[0].start) sys &= static_cast<std::uint8_t>(~0x01);
    if (in.player[1].start) sys &= static_cast<std::uint8_t>(~0x02);
    if (in.service) sys &= static_cast<std::uint8_t>(~0x10);
    if (in.player[1].coin) sys &= static_cast<std::uint8_t>(~0x40);
    if (in.player[0].coin) sys &= static_cast<std::uint8_t>(~0x80);
    system_ = sys;
    for (int p = 0; p < 2; ++p) {
        const auto& pl = in.player[static_cast<size_t>(p)];
        std::uint8_t v = 0xff;
        if (pl.right && !pl.left) v &= static_cast<std::uint8_t>(~0x01);
        if (pl.left && !pl.right) v &= static_cast<std::uint8_t>(~0x02);
        if (pl.down && !pl.up) v &= static_cast<std::uint8_t>(~0x04);
        if (pl.up && !pl.down) v &= static_cast<std::uint8_t>(~0x08);
        if (pl.button[0]) v &= static_cast<std::uint8_t>(~0x10);
        if (pl.button[1]) v &= static_cast<std::uint8_t>(~0x20);
        players_[static_cast<size_t>(p)] = v;
    }
}

std::uint8_t C1942Machine::read(std::uint16_t a) {
    const auto& rom = regions_["maincpu"];
    if (a < 0x8000) return rom[a];
    if (a < 0xc000) return rom[0x10000u + static_cast<unsigned>(bank_) * 0x4000u + (a - 0x8000u)];
    switch (a) {
        case 0xc000: return system_;
        case 0xc001: return players_[0];
        case 0xc002: return players_[1];
        case 0xc003: return dips_[0];
        case 0xc004: return dips_[1];
        default: break;
    }
    if (a >= 0xd000 && a < 0xd800) return fgram_[a - 0xd000u];
    if (a >= 0xd800 && a < 0xdc00) return bgram_[a - 0xd800u];
    if (a >= 0xe000 && a < 0xf000) return ram_[a - 0xe000u];
    return 0xff;
}

void C1942Machine::write(std::uint16_t a, std::uint8_t v) {
    if (a >= 0xe000 && a < 0xf000) { ram_[a - 0xe000u] = v; return; }
    if (a >= 0xd000 && a < 0xd800) { fgram_[a - 0xd000u] = v; return; }
    if (a >= 0xd800 && a < 0xdc00) { bgram_[a - 0xd800u] = v; return; }
    if (a >= 0xcc00 && a < 0xcc80) { spriteram_[a - 0xcc00u] = v; return; }
    switch (a) {
        case 0xc800: latch_[0] = v; break;
        case 0xc801: latch_[1] = v; break;
        case 0xc802: scroll_ = static_cast<std::uint16_t>((scroll_ & 0xff00) | v); break;
        case 0xc803: scroll_ = static_cast<std::uint16_t>((scroll_ & 0x00ff) | (v << 8)); break;
        case 0xc804: {  // bit 4: sound CPU reset, bit 7: flip
            const bool held = v & 0x10;
            if (held && !soundHeld_) {
                const auto cyc = sound_->cyc;
                z80_init(sound_.get());
                sound_->cyc = cyc;
                sound_->userdata = this;
                sound_->read_byte = [](void* u, std::uint16_t x) { return static_cast<C1942Machine*>(u)->soundRead(x); };
                sound_->write_byte = [](void* u, std::uint16_t x, std::uint8_t y) { static_cast<C1942Machine*>(u)->soundWrite(x, y); };
                sound_->port_in = [](::z80*, std::uint8_t) -> std::uint8_t { return 0xff; };
                sound_->port_out = [](::z80*, std::uint8_t, std::uint8_t) {};
            }
            soundHeld_ = held;
            flip_ = v & 0x80;
            break;
        }
        case 0xc805: paletteBank_ = v & 3; break;
        case 0xc806: bank_ = v & 3; break;
        default: break;
    }
}

std::uint8_t C1942Machine::soundRead(std::uint16_t a) {
    const auto& rom = regions_["audiocpu"];
    if (a < 0x4000) return rom[a];
    if (a >= 0x4000 && a < 0x4800) return soundRam_[a - 0x4000u];
    if (a == 0x6000) return latch_[0];
    if (a == 0x6001) return latch_[1];
    return 0xff;
}

void C1942Machine::soundWrite(std::uint16_t a, std::uint8_t v) {
    if (a >= 0x4000 && a < 0x4800) { soundRam_[a - 0x4000u] = v; return; }
    if (a == 0x8000 || a == 0x8001) ay_[0]->write(a & 1, v);
    else if (a == 0xc000 || a == 0xc001) ay_[1]->write(a & 1, v);
}

void C1942Machine::renderLine(int line) {
    const int y = line - kFirstLine;
    if (y < 0 || y >= kHeight) return;
    std::uint32_t* out = &frame_[static_cast<size_t>(y * kWidth)];
    const auto& tileLut = regions_["tileprom"];
    const auto& sprLut = regions_["sprprom"];
    const auto& charLut = regions_["charprom"];
    auto lut = [](const std::vector<std::uint8_t>& p, size_t i) { return i < p.size() ? p[i] & 0x0f : 0; };

    // Background: 32x16 tiles of 16x16, column-major, scrolled horizontally.
    const int vy = line & 0xff;
    const size_t nTiles = tiles_.size() / 256;
    for (int x = 0; x < kWidth; ++x) {
        const int tx = (x + scroll_) & 0x1ff;
        const int ti = (tx >> 4) * 16 + (vy >> 4);
        const int idx = (ti & 0x0f) | ((ti & 0x1f0) << 1);
        const std::uint8_t attr = bgram_[static_cast<size_t>(idx + 0x10)];
        const int code = bgram_[static_cast<size_t>(idx)] + ((attr & 0x80) << 1);
        int px = tx & 15, py = vy & 15;
        if (attr & 0x20) px = 15 - px;
        if (attr & 0x40) py = 15 - py;
        const int pen = nTiles ? tiles_[(static_cast<size_t>(code) % nTiles) * 256 + static_cast<size_t>(py * 16 + px)] : 0;
        const int colour = attr & 0x1f;
        out[x] = rgb_[static_cast<size_t>((paletteBank_ << 4) | lut(tileLut, static_cast<size_t>(colour * 8 + pen)))];
    }

    // Sprites: the hardware's per-line object scan (MAME draw_sprites).
    const size_t nSpr = sprites_.size() / 256;
    const auto v = static_cast<std::uint8_t>(flip_ ? ~(line - 1) : line - 1);
    for (int h = 496; h >= 128; h -= 16) {
        const bool objcnt4 = ((h >> 8) & 1) != ((~h >> 7) & 1);
        const bool objcnt3 = (((v >> 7) & 1) && objcnt4) != (((~h) >> 7) & 1);
        int objIdx = (h >> 4) & 7;
        objIdx |= objcnt3 ? 0x08 : 0;
        objIdx |= objcnt4 ? 0x10 : 0;
        objIdx <<= 2;
        const std::uint8_t* d = &spriteram_[static_cast<size_t>(objIdx)];
        int code = (d[0] & 0x7f) | (((d[1] >> 5) & 1) << 7) | (((d[0] >> 7) & 1) << 8);
        const int col = d[1] & 0x0f;
        int sx = d[3] - (((d[1] >> 4) & 1) << 8);
        int sy = d[2];
        const auto valpha = static_cast<std::uint8_t>(sy);
        const auto v2c = static_cast<std::uint8_t>(static_cast<std::uint8_t>(~v) + (flip_ ? 0x01 : 0xff));
        const auto lvbeta = static_cast<std::uint8_t>(v2c + valpha);
        const auto vbeta = static_cast<std::uint8_t>(~lvbeta);
        const bool vleq = vbeta <= static_cast<std::uint8_t>(~valpha);
        const int vlen = d[1] >> 6;
        bool vinlen = true;
        switch (vlen & 3) {
            case 0: vinlen = (lvbeta & 0xf0) == 0xf0; break;
            case 1: vinlen = (lvbeta & 0xe0) == 0xe0; break;
            case 2: vinlen = (lvbeta & 0xc0) == 0xc0; break;
            default: vinlen = true; break;
        }
        if (!(vleq && vinlen)) continue;  // not on this line
        int dir = 1;
        if (flip_) { sx = 240 - sx; sy = 240 - sy; dir = -1; }
        const int rows = vlen == 3 ? 16 : (1 << vlen);
        code &= ~(rows - 1);
        for (int i = 0; i < rows; ++i) {
            const int top = sy + 16 * i * dir;
            int r = line - top;
            if (r < 0 || r > 15) continue;
            if (flip_) r = 15 - r;
            const size_t base = (static_cast<size_t>(code + i) % std::max<size_t>(nSpr, 1)) * 256 + static_cast<size_t>(r * 16);
            if (!nSpr) break;
            for (int k = 0; k < 16; ++k) {
                const int x = sx + k;
                if (x < 0 || x >= kWidth) continue;
                const int pen = sprites_[base + static_cast<size_t>(flip_ ? 15 - k : k)];
                if (pen == 15) continue;
                out[x] = rgb_[static_cast<size_t>(0x40 | lut(sprLut, static_cast<size_t>(col * 16 + pen)))];
            }
        }
    }

    // Foreground text: 32x32 characters of 8x8, pen 0 transparent.
    const size_t nChars = chars_.size() / 64;
    const int fy = line & 0xff;
    for (int x = 0; x < kWidth; ++x) {
        const int ti = (fy >> 3) * 32 + (x >> 3);
        const std::uint8_t attr = fgram_[static_cast<size_t>(ti + 0x400)];
        const int code = fgram_[static_cast<size_t>(ti)] + ((attr & 0x80) << 1);
        const int pen = nChars ? chars_[(static_cast<size_t>(code) % nChars) * 64 + static_cast<size_t>((fy & 7) * 8 + (x & 7))] : 0;
        if (pen == 0) continue;
        out[x] = rgb_[static_cast<size_t>(0x80 | lut(charLut, static_cast<size_t>((attr & 0x3f) * 4 + pen)))];
    }
}

void C1942Machine::runFrame() {
    const double cpuPerLine = kCpuClock * kHTotal / kPixelClock;  // 192 cycles
    const double ayPerLine = kAyClock / 8.0 * kHTotal / kPixelClock;
    const auto& irqProm = regions_["irqprom"];
    for (int line = 0; line < kVTotal; ++line) {
        const std::uint8_t irq = static_cast<size_t>(line & 0xff) < irqProm.size() ? irqProm[static_cast<size_t>(line & 0xff)] : 0;
        if (irq & 8) z80_gen_int(main_.get(), static_cast<std::uint8_t>(0xc7 | ((irq << 3) & 0x18)));  // RST 08h / 10h
        if ((irq & 4) && !soundHeld_) z80_gen_int(sound_.get(), 0xff);                                // IM 1
        mainTarget_ += cpuPerLine;
        while (static_cast<double>(main_->cyc) < mainTarget_) z80_step(main_.get());
        soundTarget_ += cpuPerLine;
        if (soundHeld_) sound_->cyc = static_cast<unsigned long>(soundTarget_);
        else while (static_cast<double>(sound_->cyc) < soundTarget_) z80_step(sound_.get());
        renderLine(line);

        // Two AY-3-8910s, mixed to mono, a gentle low-pass for the speaker.
        ayAcc_ += ayPerLine;
        while (ayAcc_ >= 4.0) {
            ayAcc_ -= 4.0;
            int sum = 0;
            for (int k = 0; k < 4; ++k) sum += ay_[0]->step() + ay_[1]->step();
            const double s = sum / 4.0 / 6.0;
            lp_ += 0.45 * (s - lp_);
            const auto o = static_cast<std::int16_t>(std::clamp(lp_ * 0.9, -32767.0, 32767.0));
            audio_.push_back(o);
            audio_.push_back(o);
        }
    }
    if (main_->cyc > 1'000'000'000UL) {
        main_->cyc -= 900'000'000UL;
        mainTarget_ -= 900'000'000.0;
        sound_->cyc -= 900'000'000UL;
        soundTarget_ -= 900'000'000.0;
    }
}

void C1942Machine::takeAudio(std::vector<std::int16_t>& out) {
    out.insert(out.end(), audio_.begin(), audio_.end());
    audio_.clear();
}

// ------------------------------------------------------------------ module

namespace {
using emu::RomFile;
using emu::RomLoad;

struct Def {
    emu::GameInfo info;
    std::vector<RomFile> roms;
};

struct RawDef {
    const char* set;
    const char* parent;
    const char* title;
    std::vector<RomFile> roms;
};

const std::vector<Def>& defs() {
    static const std::vector<Def> d = [] {
        const std::vector<RawDef> raw = {
#include "c1942_roms.inc"
        };
        std::vector<Def> v;
        for (const auto& r : raw) {
            Def x;
            x.info.set = r.set;
            x.info.parent = r.parent;
            x.info.title = r.title;
            if (x.info.parent.empty()) x.info.title = "1942";
            x.info.year = std::string(r.set) == "1942w" ? "1985" : "1984";
            x.info.manufacturer = std::string(r.set) == "1942w" ? "Capcom (Williams Electronics license)" : "Capcom";
            x.info.system = "Capcom 1942";
            x.info.players = 2;
            x.info.buttons = 2;
            x.info.rotation = 270;
            x.info.joystick = "8-way";
            x.info.buttonNames = {"Shot", "Loop"};
            x.roms = r.roms;
            v.push_back(std::move(x));
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
    auto loaded = emu::loadRomSet(gamesDir, d->info.set, d->info.parent,
                                  {{"maincpu", 0x20000, 0xff}, {"audiocpu", 0x10000}, {"gfx1", 0x2000}, {"gfx2", 0xc000},
                                   {"gfx3", 0x10000}, {"palproms", 0x300}, {"charprom", 0x100}, {"tileprom", 0x100},
                                   {"sprprom", 0x100}, {"irqprom", 0x100}},
                                  d->roms);
    if (!loaded.ok) {
        if (error) {
            *error = "ROM set '" + set + "' is incomplete or a different version. Missing or wrong:";
            for (const auto& m : loaded.missing) *error += "\n  " + m;
        }
        return nullptr;
    }
    // DIP defaults from MAME: DSWA 0xf7 (upright), DSWB 0xff.
    return std::make_unique<C1942Machine>(d->info, std::move(loaded.regions), std::array<std::uint8_t, 2>{0xf7, 0xff});
}

}  // namespace zen1::c1942
