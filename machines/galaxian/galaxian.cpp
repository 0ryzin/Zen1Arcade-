// SPDX-License-Identifier: GPL-3.0-or-later
//
// Namco Galaxian hardware (1979): Z80 at 3.072 MHz, 32x32 tile layer with
// per-column scroll and colour, eight 16x16 sprites, shells and a missile
// drawn in hardware, the LFSR star field, and analogue sound.
//
// Video rules (tile/sprite/bullet placement, star generator, palette) follow
// MAME's galaxian driver (src/mame/galaxian, BSD-3-Clause, copyright-holders:
// Aaron Giles, Couriersud, Stephane Humbert, Robbbert). The sound board is
// analogue; Zen1 approximates it with its own synthesiser (background swarm,
// melody, shot and explosion), not a circuit simulation.
#include "galaxian.hpp"

extern "C" {
#include "z80.h"
}

#include <algorithm>
#include <cmath>

namespace zen1::galaxian {

namespace {
constexpr double kCpuClock = 3'072'000.0;
constexpr int kLines = 264;
constexpr double kCyclesPerLine = 192.0;  // 384 pixel clocks at 6.144 MHz / 2
constexpr int kStarPeriod = (1 << 17) - 1;
constexpr int kRate = 48000;
constexpr double kPi = 3.14159265358979323846;
}  // namespace

// ----------------------------------------------------------------- sound

void Sound::reset() { *this = Sound{}; }

void Sound::latch(int bit, bool on) {
    if (bit == 3 && on) hitAmp_ = 1.0;
    if (bit == 5 && on && !fire_) fireT_ = 0.0;
    switch (bit) {
    case 0: fs_[0] = on; break;
    case 1: fs_[1] = on; break;
    case 2: fs_[2] = on; break;
    case 3: hit_ = on; break;
    case 5: fire_ = on; break;
    case 6: vol1_ = on; break;
    case 7: vol2_ = on; break;
    default: break;
    }
}

void Sound::lfo(int bit, bool on) {
    if (on) lfoDac_ |= 1 << bit;
    else lfoDac_ &= ~(1 << bit);
}

double Sound::sample() {
    const double dt = 1.0 / kRate;
    double out = 0.0;
    // Melody: the pitch register divides 1.536 MHz into a 4-bit counter.
    if (pitch_ != 0xff) {
        const double f = 1'536'000.0 / (256 - pitch_) / 16.0;
        tonePh_ += f * dt;
        tonePh_ -= std::floor(tonePh_);
        double t = (tonePh_ < 0.5 ? 1.0 : -1.0) * 0.5;
        if (vol1_) t += (std::fmod(tonePh_ * 2, 1.0) < 0.5 ? 0.25 : -0.25);
        if (vol2_) t += (std::fmod(tonePh_ * 8, 1.0) < 0.5 ? 0.15 : -0.15);
        out += t * 0.22;
    }
    // Background "swarm": three 555 oscillators swept by the LFO.
    lfoPh_ += (0.4 + lfoDac_ * 0.25) * dt;
    lfoPh_ -= std::floor(lfoPh_);
    static const double base[3] = {340.0, 430.0, 520.0};
    for (int i = 0; i < 3; ++i) {
        if (!fs_[i]) continue;
        fsPh_[i] += base[i] * (1.0 + 0.45 * lfoPh_) * dt;
        fsPh_[i] -= std::floor(fsPh_[i]);
        out += (fsPh_[i] < 0.5 ? 0.06 : -0.06);
    }
    // Noise source (LFSR) shared by shot and explosion.
    noiseCount_ += 1;
    if (noiseCount_ >= 3) {
        noiseCount_ = 0;
        const unsigned bit = ((lfsr_ >> 0) ^ (lfsr_ >> 2)) & 1;
        lfsr_ = (lfsr_ >> 1) | (bit << 16);
    }
    const double noise = (lfsr_ & 1) ? 1.0 : -1.0;
    noiseLp_ += 0.15 * (noise - noiseLp_);
    // Explosion: filtered noise that rings on while HIT is held, then decays.
    if (!hit_) hitAmp_ *= std::exp(-dt / 0.35);
    out += noiseLp_ * hitAmp_ * 0.55;
    // Shot: a falling square with noise.
    if (fireT_ >= 0.0) {
        const double len = 0.28;
        const double f = 2400.0 * std::pow(0.25, fireT_ / len);
        firePh_ += f * dt;
        firePh_ -= std::floor(firePh_);
        const double env = std::max(0.0, 1.0 - fireT_ / len);
        out += ((firePh_ < 0.5 ? 1.0 : -1.0) * 0.18 + noise * 0.06) * env;
        fireT_ += dt;
        if (fireT_ > len) fireT_ = -1.0;
    }
    return out;
}

// ----------------------------------------------------------------- machine

GalaxianMachine::GalaxianMachine(const emu::GameInfo& info, emu::RegionMap regions, std::array<std::uint8_t, 3> ports)
    : info_(info), regions_(std::move(regions)), ports_(ports) {
    rom_ = regions_["maincpu"];
    rom_.resize(0x4000, 0xff);
    decodeGfx();
    buildPalette();
    // Star field: 2^17-1 step LFSR, enabled where the top 8 bits are 1 and bit 0 is 0.
    stars_.resize(kStarPeriod);
    std::uint32_t sr = 0;
    for (int i = 0; i < kStarPeriod; ++i) {
        const bool on = (sr & 0x1fe01) == 0x1fe00;
        stars_[static_cast<size_t>(i)] = static_cast<std::uint8_t>(((~sr & 0x1f8) >> 3) | (on ? 0x80 : 0));
        sr = (sr >> 1) | ((((sr >> 12) ^ ~sr) & 1) << 16);
    }
    frame_.assign(static_cast<size_t>(kWidth * kHeight), 0);
    cpu_ = std::make_unique<::z80>();
    reset();
}

GalaxianMachine::~GalaxianMachine() = default;

emu::VideoInfo GalaxianMachine::video() const {
    return emu::VideoInfo{kWidth, kHeight, 4.0 / 3.0, info_.rotation, 6'144'000.0 / 384.0 / 264.0};
}

void GalaxianMachine::reset() {
    z80_init(cpu_.get());
    cpu_->userdata = this;
    cpu_->read_byte = [](void* u, std::uint16_t a) { return static_cast<GalaxianMachine*>(u)->read(a); };
    cpu_->write_byte = [](void* u, std::uint16_t a, std::uint8_t v) { static_cast<GalaxianMachine*>(u)->write(a, v); };
    cpu_->port_in = [](::z80*, std::uint8_t) -> std::uint8_t { return 0xff; };
    cpu_->port_out = [](::z80*, std::uint8_t, std::uint8_t) {};
    ram_.fill(0);
    vram_.fill(0);
    obj_.fill(0);
    irqEnabled_ = false;
    starsEnabled_ = false;
    starOrigin_ = 0;
    sound_.reset();
    audio_.clear();
}

void GalaxianMachine::setInput(const emu::CabinetInput& in) {
    // Active-high inputs. Upright cabinet: both panels drive the one control set.
    std::uint8_t i0 = ports_[0], i1 = ports_[1];
    for (const auto& p : in.player) {
        if (p.left) i0 |= 0x04;
        if (p.right) i0 |= 0x08;
        if (p.button[0]) i0 |= 0x10;
    }
    if (in.player[0].coin) i0 |= 0x01;
    if (in.player[1].coin) i0 |= 0x02;
    if (in.player[0].start) i1 |= 0x01;
    if (in.player[1].start) i1 |= 0x02;
    in0_ = i0;
    in1_ = i1;
}

std::uint8_t GalaxianMachine::read(std::uint16_t a) {
    if (a < 0x4000) return rom_[a];
    if (a < 0x4800) return ram_[a & 0x3ff];
    if (a >= 0x5000 && a < 0x5800) return vram_[a & 0x3ff];
    if (a >= 0x5800 && a < 0x6000) return obj_[a & 0xff];
    if (a >= 0x6000 && a < 0x6800) return in0_;
    if (a >= 0x6800 && a < 0x7000) return in1_;
    if (a >= 0x7000 && a < 0x7800) return ports_[2];
    return 0xff;  // 0x7800 watchdog, unmapped
}

void GalaxianMachine::write(std::uint16_t a, std::uint8_t v) {
    if (a < 0x4000) return;
    if (a < 0x4800) { ram_[a & 0x3ff] = v; return; }
    if (a >= 0x5000 && a < 0x5800) { vram_[a & 0x3ff] = v; return; }
    if (a >= 0x5800 && a < 0x6000) { obj_[a & 0xff] = v; return; }
    if (a >= 0x6000 && a < 0x6800) {
        const int bit = a & 7;
        if (bit >= 4) sound_.lfo(bit - 4, v & 1);
        return;  // 0-3: lamps, coin lock, coin counter
    }
    if (a >= 0x6800 && a < 0x7000) { sound_.latch(a & 7, v & 1); return; }
    if (a >= 0x7000 && a < 0x7800) {
        switch (a & 7) {
        case 1:
            irqEnabled_ = v & 1;
            if (!irqEnabled_) cpu_->nmi_pending = false;
            break;
        case 4: starsEnabled_ = v & 1; break;
        default: break;  // flip screen (cocktail only)
        }
        return;
    }
    if (a >= 0x7800) sound_.pitch(v);
}

void GalaxianMachine::decodeGfx() {
    const auto& g = regions_["gfx1"];
    const size_t half = g.size() / 2;
    auto pen = [&](size_t byte, int bit) {
        const int b0 = (g[byte] >> (7 - bit)) & 1, b1 = (g[byte + half] >> (7 - bit)) & 1;
        return static_cast<std::uint8_t>((b0 << 1) | b1);
    };
    const int nTiles = static_cast<int>(half / 8);
    tiles_.assign(static_cast<size_t>(nTiles) * 64, 0);
    for (int t = 0; t < nTiles; ++t)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                tiles_[static_cast<size_t>(t * 64 + y * 8 + x)] = pen(static_cast<size_t>(t * 8 + y), x);
    const int nSprites = static_cast<int>(half / 32);
    sprites_.assign(static_cast<size_t>(nSprites) * 256, 0);
    for (int s = 0; s < nSprites; ++s)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const size_t byte = static_cast<size_t>(s * 32 + (y & 7) + (x >= 8 ? 8 : 0) + (y >= 8 ? 16 : 0));
                sprites_[static_cast<size_t>(s * 256 + y * 16 + x)] = pen(byte, x & 7);
            }
}

void GalaxianMachine::buildPalette() {
    const auto& p = regions_["proms"];
    for (size_t i = 0; i < 32 && i < p.size(); ++i) {
        const std::uint8_t c = p[i];
        const int r = 0x21 * (c & 1) + 0x47 * ((c >> 1) & 1) + 0x97 * ((c >> 2) & 1);
        const int g = 0x21 * ((c >> 3) & 1) + 0x47 * ((c >> 4) & 1) + 0x97 * ((c >> 5) & 1);
        const int b = 0x51 * ((c >> 6) & 1) + 0xae * ((c >> 7) & 1);
        rgb_[i] = static_cast<std::uint32_t>(r << 16 | g << 8 | b);
    }
    // Star colours: two resistors per channel, compressed into the bright range.
    static const int starmap[4] = {0, 194, 214, 255};
    for (int i = 0; i < 64; ++i) {
        const int r = starmap[((i >> 4) & 1) << 1 | ((i >> 5) & 1)];
        const int g = starmap[((i >> 2) & 1) << 1 | ((i >> 3) & 1)];
        const int b = starmap[(i & 1) << 1 | ((i >> 1) & 1)];
        starRgb_[static_cast<size_t>(i)] = static_cast<std::uint32_t>(r << 16 | g << 8 | b);
    }
}

void GalaxianMachine::render() {
    std::fill(frame_.begin(), frame_.end(), 0u);
    // Stars (screen lines 16..239 are the visible ones).
    if (starsEnabled_) {
        for (int row = 0; row < kHeight; ++row) {
            const int y = row + 16;
            std::uint32_t offs = (starOrigin_ + static_cast<std::uint32_t>(y) * 512) % kStarPeriod;
            for (int x = 0; x < 256; ++x) {
                const bool en = ((y ^ (x >> 3)) & 1) != 0;
                for (int k = 0; k < 2; ++k) {
                    const std::uint8_t s = stars_[offs];
                    offs = offs + 1 >= kStarPeriod ? 0 : offs + 1;
                    if (en && (s & 0x80) && k == 1) frame_[static_cast<size_t>(row * kWidth + x)] = starRgb_[s & 0x3f];
                }
            }
        }
    }
    // Tiles: 32x32, each column scrolled vertically, colour per column.
    for (int x = 0; x < 256; ++x) {
        const int col = x >> 3;
        const int scroll = obj_[static_cast<size_t>(col * 2)];
        const int colour = obj_[static_cast<size_t>(col * 2 + 1)] & 7;
        for (int row = 0; row < kHeight; ++row) {
            const int ty = (row + 16 + scroll) & 255;
            const int code = vram_[static_cast<size_t>((ty >> 3) * 32 + col)];
            const int pen = tiles_[static_cast<size_t>((code % (static_cast<int>(tiles_.size()) / 64)) * 64 + (ty & 7) * 8 + (x & 7))];
            if (pen) frame_[static_cast<size_t>(row * kWidth + x)] = rgb_[static_cast<size_t>(colour * 4 + pen)];
        }
    }
    // Sprites: 8 at 0x40, lower numbers in front; the first 16 pixels are clipped.
    for (int n = 7; n >= 0; --n) {
        const std::uint8_t* b = &obj_[static_cast<size_t>(0x40 + n * 4)];
        const int sy = static_cast<std::uint8_t>(240 - (b[0] - (n < 3 ? 1 : 0)));
        const int code = b[1] & 0x3f;
        const bool fx = b[1] & 0x40, fy = b[1] & 0x80;
        const int colour = b[2] & 7;
        const int sx = static_cast<std::uint8_t>(b[3] + 1);
        for (int y = 0; y < 16; ++y) {
            const int row = sy + y - 16;
            if (row < 0 || row >= kHeight) continue;
            for (int x = 0; x < 16; ++x) {
                const int px = sx + x;
                if (px < 17 || px >= 256) continue;
                const int pen = sprites_[static_cast<size_t>(code * 256 + (fy ? 15 - y : y) * 16 + (fx ? 15 - x : x))];
                if (pen) frame_[static_cast<size_t>(row * kWidth + px)] = rgb_[static_cast<size_t>(colour * 4 + pen)];
            }
        }
    }
    // Shells (white) and the missile (yellow): 4 pixels each, one of each per line.
    const std::uint8_t* base = &obj_[0x60];
    for (int row = 0; row < kHeight; ++row) {
        const int y = row + 16;
        int shell = -1, missile = -1;
        for (int w = 0; w < 3; ++w)
            if (static_cast<std::uint8_t>(base[w * 4 + 1] + (y - 1)) == 0xff) shell = w;
        for (int w = 3; w < 8; ++w)
            if (static_cast<std::uint8_t>(base[w * 4 + 1] + y) == 0xff) (w == 7 ? missile : shell) = w;
        for (int which : {shell, missile}) {
            if (which < 0) continue;
            const int x0 = 255 - base[which * 4 + 3] - 4;
            const std::uint32_t c = which == 7 ? 0xffff00u : 0xffffffu;
            for (int k = 0; k < 4; ++k)
                if (x0 + k >= 0 && x0 + k < 256) frame_[static_cast<size_t>(row * kWidth + x0 + k)] = c;
        }
    }
}

void GalaxianMachine::runFrame() {
    ::z80* z = cpu_.get();
    double target = static_cast<double>(z->cyc);
    const double samplesPerCycle = kRate / kCpuClock;
    for (int line = 0; line < kLines; ++line) {
        if (line == 240) {  // VBLANK: NMI when enabled
            render();
            if (irqEnabled_) z80_gen_nmi(z);
            // The star generator runs one step short per frame: stars drift sideways.
            starOrigin_ = (starOrigin_ + kStarPeriod - 1) % kStarPeriod;
        }
        target += kCyclesPerLine;
        while (static_cast<double>(z->cyc) < target) {
            const unsigned long before = z->cyc;
            z80_step(z);
            sampleAcc_ += static_cast<double>(z->cyc - before) * samplesPerCycle;
            while (sampleAcc_ >= 1.0) {
                sampleAcc_ -= 1.0;
                const auto v = static_cast<std::int16_t>(std::clamp(sound_.sample() * 26000.0, -32000.0, 32000.0));
                audio_.push_back(v);
                audio_.push_back(v);
            }
        }
    }
    if (z->cyc > 1'000'000'000UL) z->cyc -= 900'000'000UL;
}

void GalaxianMachine::takeAudio(std::vector<std::int16_t>& out) {
    out.insert(out.end(), audio_.begin(), audio_.end());
    audio_.clear();
}

// ----------------------------------------------------------------- games

namespace {
using emu::RomLoad;
struct Def {
    emu::GameInfo info;
    std::array<std::uint8_t, 3> ports;  // IN0, IN1, IN2 defaults (DIP switches)
    std::vector<emu::RomFile> roms;
};

const std::vector<Def>& defs() {
    static const std::vector<Def> d = [] {
        auto info = [](const char* set, const char* parent, const char* title, const char* maker) {
            emu::GameInfo g;
            g.set = set; g.parent = parent; g.title = title; g.year = "1979"; g.manufacturer = maker;
            g.system = "Namco Galaxian"; g.players = 2; g.buttons = 1; g.rotation = 90;
            g.joystick = "2-way";
            g.buttonNames = {"Fire"};
            return g;
        };
        const emu::RomFile prom{"proms", "6l.bpr", 0xc3ac9467, 0x0000, 0x0020, RomLoad::Plain};
        std::vector<Def> v;
        v.push_back({info("galaxian", "", "Galaxian", "Namco"), {0x00, 0x00, 0x04}, {
            {"maincpu", "galmidw.u", 0x745e2d61, 0x0000, 0x0800, RomLoad::Plain},
            {"maincpu", "galmidw.v", 0x9c999a40, 0x0800, 0x0800, RomLoad::Plain},
            {"maincpu", "galmidw.w", 0xb5894925, 0x1000, 0x0800, RomLoad::Plain},
            {"maincpu", "galmidw.y", 0x6b3ca10b, 0x1800, 0x0800, RomLoad::Plain},
            {"maincpu", "7l", 0x1b933207, 0x2000, 0x0800, RomLoad::Plain},
            {"gfx1", "1h.bin", 0x39fb43a4, 0x0000, 0x0800, RomLoad::Plain},
            {"gfx1", "1k.bin", 0x7e3f56a2, 0x0800, 0x0800, RomLoad::Plain}, prom}});
        v.push_back({info("galaxiana", "galaxian", "Galaxian (set 2)", "Namco"), {0x00, 0x00, 0x01}, {
            {"maincpu", "7f.bin", 0x4335b1de, 0x0000, 0x1000, RomLoad::Plain},
            {"maincpu", "7j.bin", 0x4e6f66a1, 0x1000, 0x1000, RomLoad::Plain},
            {"maincpu", "7l.bin", 0x5341d75a, 0x2000, 0x0800, RomLoad::Plain},
            {"gfx1", "1h.bin", 0x39fb43a4, 0x0000, 0x0800, RomLoad::Plain},
            {"gfx1", "1k.bin", 0x7e3f56a2, 0x0800, 0x0800, RomLoad::Plain}, prom}});
        v.push_back({info("galaxianm", "galaxian", "Galaxian (Midway)", "Namco (Midway license)"), {0x00, 0x00, 0x04}, {
            {"maincpu", "galmidw.u", 0x745e2d61, 0x0000, 0x0800, RomLoad::Plain},
            {"maincpu", "galmidw.v", 0x9c999a40, 0x0800, 0x0800, RomLoad::Plain},
            {"maincpu", "galmidw.w", 0xb5894925, 0x1000, 0x0800, RomLoad::Plain},
            {"maincpu", "galmidw.y", 0x6b3ca10b, 0x1800, 0x0800, RomLoad::Plain},
            {"maincpu", "galmidw.z", 0xcb24f797, 0x2000, 0x0800, RomLoad::Plain},
            {"gfx1", "galaxian.j1", 0x84decf98, 0x0000, 0x0800, RomLoad::Plain},
            {"gfx1", "galaxian.l1", 0xc31ada9e, 0x0800, 0x0800, RomLoad::Plain}, prom}});
        v.push_back({info("galaxiant", "galaxian", "Galaxian (Taito)", "Namco (Taito license)"), {0x00, 0x00, 0x01}, {
            {"maincpu", "gl-03.8g", 0xe8f3aa67, 0x0000, 0x0800, RomLoad::Plain},
            {"maincpu", "gl-04.8f", 0xf58283e3, 0x0800, 0x0800, RomLoad::Plain},
            {"maincpu", "gl-05.8e", 0x4c7031c0, 0x1000, 0x0800, RomLoad::Plain},
            {"maincpu", "gl-06.8d", 0x097d92a2, 0x1800, 0x0800, RomLoad::Plain},
            {"maincpu", "gl-07.8c", 0x5341d75a, 0x2000, 0x0800, RomLoad::Plain},
            {"gfx1", "gl-02.1k", 0xd14f7510, 0x0000, 0x0800, RomLoad::Plain},
            {"gfx1", "gl-01.1j", 0x968b6016, 0x0800, 0x0800, RomLoad::Plain}, prom}});
        v.push_back({info("superg", "galaxian", "Super Galaxians", "hack"), {0x00, 0x00, 0x01}, {
            {"maincpu", "7f.bin", 0x4335b1de, 0x0000, 0x1000, RomLoad::Plain},
            {"maincpu", "superg.w", 0xddeabdae, 0x1000, 0x0800, RomLoad::Plain},
            {"maincpu", "superg.y", 0x9463f753, 0x1800, 0x0800, RomLoad::Plain},
            {"maincpu", "superg.z", 0xe6312e35, 0x2000, 0x0800, RomLoad::Plain},
            {"gfx1", "galmidw.1j", 0x84decf98, 0x0000, 0x0800, RomLoad::Plain},
            {"gfx1", "galmidw.1k", 0xc31ada9e, 0x0800, 0x0800, RomLoad::Plain}, prom}});
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
                                  {{"maincpu", 0x4000, 0xff}, {"gfx1", 0x1000}, {"proms", 0x20}}, d->roms);
    if (!loaded.ok) {
        if (error) {
            *error = "ROM set '" + set + "' is incomplete or a different version. Missing or wrong:";
            for (const auto& m : loaded.missing) *error += "\n  " + m;
        }
        return nullptr;
    }
    return std::make_unique<GalaxianMachine>(d->info, std::move(loaded.regions), d->ports);
}

}  // namespace zen1::galaxian
