// SPDX-License-Identifier: GPL-3.0-or-later
#include "neogeo.hpp"

#include <Moira.h>
#include <ymfm_opn.h>

extern "C" {
#include <z80.h>
}

#include <algorithm>
#include <cmath>
#include <ctime>

namespace zen1::neogeo {

namespace {
constexpr double kMasterClock = 24'000'000.0;
constexpr double kPixelClock = kMasterClock / 4;   // 6 MHz
constexpr double kZ80Clock = kMasterClock / 6;     // 4 MHz
constexpr double kYmClock = kMasterClock / 3;      // 8 MHz
constexpr int kHTotal = 384, kVTotal = 264;
constexpr int kVBlankStart = 240, kVisibleTop = 16, kAutoAnimLine = 256;
constexpr int kCpuPerPixel = 2;                    // 68000 at 12 MHz
constexpr int kCpuPerLine = kHTotal * kCpuPerPixel;
constexpr int kVblankIrqPixel = (56 + 2) / 4;      // 14 pixels into line 240
constexpr int kReloadPixel = 1146 / 4;             // display counter reload point
constexpr int kYmPrescale = 144;                   // input clocks per FM sample

int bit(std::uint32_t v, int b) { return (v >> b) & 1; }

// Horizontal shrink patterns (from MAME, verified on hardware there).
constexpr std::uint16_t kZoomX[16] = {0x0080, 0x0880, 0x0888, 0x2888, 0x288a, 0x2a8a, 0x2aaa, 0xaaaa,
                                      0xaaea, 0xbaea, 0xbaeb, 0xbbeb, 0xbbef, 0xfbef, 0xfbff, 0xffff};
}  // namespace

// ------------------------------------------------------------------ uPD4990A

void Upd4990a::write(bool din, bool clk, bool stb) {
    if (clk && !clk_) {  // rising edge: shift in, LSB out
        shift_ = (shift_ >> 1) | (static_cast<std::uint64_t>(din) << 51);
    }
    if (stb && !stb_) {  // rising edge: execute the command in the top 4 bits
        const int cmd = static_cast<int>((shift_ >> 48) & 0xf);
        switch (cmd) {
        case 0x0: mode_ = 0; break;  // register hold
        case 0x1: mode_ = 1; break;  // shift
        case 0x2: mode_ = 2; break;  // time set (ignored: the host clock is used)
        case 0x3: {                  // time read: load the current time
            mode_ = 3;
            const std::time_t t = std::time(nullptr);
            std::tm tm{};
            localtime_r(&t, &tm);
            auto bcd = [](int v) { return static_cast<std::uint64_t>(((v / 10) << 4) | (v % 10)); };
            shift_ = bcd(tm.tm_sec) | bcd(tm.tm_min) << 8 | bcd(tm.tm_hour) << 16 | bcd(tm.tm_mday) << 24 |
                     static_cast<std::uint64_t>(tm.tm_wday) << 32 | static_cast<std::uint64_t>(tm.tm_mon + 1) << 36 |
                     bcd(tm.tm_year % 100) << 40;
            break;
        }
        case 0x4: tpHz_ = 64; break;
        case 0x5: tpHz_ = 256; break;
        case 0x6: tpHz_ = 2048; break;
        case 0x7: tpHz_ = 4096; break;
        case 0x8: tpHz_ = 1; break;
        case 0x9: tpHz_ = 0.1; break;
        case 0xa: tpHz_ = 1.0 / 30; break;
        case 0xb: tpHz_ = 1.0 / 60; break;
        default: break;
        }
    }
    clk_ = clk;
    stb_ = stb;
}

bool Upd4990a::dataOut(double seconds) const {
    if (mode_ == 1 || mode_ == 3) return shift_ & 1;
    return std::fmod(seconds, 1.0) < 0.5;  // 1 Hz signal in hold mode
}

bool Upd4990a::tp(double seconds) const { return std::fmod(seconds * tpHz_, 1.0) < 0.5; }

// ------------------------------------------------------------------ 68000

struct NeoGeoMachine::Cpu68k : moira::Moira {
    NeoGeoMachine* m = nullptr;
    moira::u8 read8(moira::u32 a) const override { return m->read8(a & 0xffffff); }
    moira::u16 read16(moira::u32 a) const override { return m->read16(a & 0xffffff); }
    void write8(moira::u32 a, moira::u8 v) const override { m->write8(a & 0xffffff, v); }
    void write16(moira::u32 a, moira::u16 v) const override { m->write16(a & 0xffffff, v); }
    moira::u32 read32(moira::u32 a) const override { return (static_cast<moira::u32>(read16(a)) << 16) | read16(a + 2); }
    void write32(moira::u32 a, moira::u32 v) const override {
        write16(a, static_cast<moira::u16>(v >> 16));
        write16(a + 2, static_cast<moira::u16>(v));
    }
    // Neo Geo interrupts stay pending until the program acknowledges them
    // through the LSPC (0x3c000c); the 68000 uses autovectors.
    moira::IrqMode readIrqVector(moira::u8, moira::u8*) const override { return moira::IrqMode::AUTO; }
};

// ------------------------------------------------------------------ YM2610

struct NeoGeoMachine::Ym : ymfm::ymfm_interface {
    ymfm::ym2610 chip{*this};
    const std::vector<std::uint8_t>* a = nullptr;
    const std::vector<std::uint8_t>* b = nullptr;
    std::int32_t timer[2] = {-1, -1};
    bool irq = false;

    std::uint8_t ymfm_external_read(ymfm::access_class type, std::uint32_t address) override {
        const auto* rom = type == ymfm::ACCESS_ADPCM_A ? a : type == ymfm::ACCESS_ADPCM_B ? b : nullptr;
        if (!rom || rom->empty()) return 0;
        return (*rom)[address % rom->size()];
    }
    void ymfm_set_timer(std::uint32_t t, std::int32_t clocks) override { timer[t & 1] = clocks; }
    void ymfm_update_irq(bool asserted) override { irq = asserted; }
    void advance(std::int32_t clocks) {
        for (std::uint32_t t = 0; t < 2; ++t) {
            if (timer[t] < 0) continue;
            timer[t] -= clocks;
            if (timer[t] <= 0) {
                timer[t] = -1;
                m_engine->engine_timer_expired(t);
            }
        }
    }
};

// ------------------------------------------------------------------ machine

NeoGeoMachine::NeoGeoMachine(const emu::GameInfo& info, emu::RegionMap regions)
    : info_(info), regions_(std::move(regions)) {
    prog_ = &regions_["maincpu"];
    bios_ = &regions_["bios"];
    fixCart_ = &regions_["fixed"];
    fixBios_ = &regions_["fixedbios"];
    zoomY_ = &regions_["zoomy"];
    z80Bios_ = &regions_["audiobios"];
    adpcmA_ = &regions_["adpcma"];
    adpcmB_ = regions_.count("adpcmb") && !regions_["adpcmb"].empty() ? &regions_["adpcmb"] : adpcmA_;

    // Z80 cartridge ROM: M1 at 0 and again from 0x10000, the base of the
    // NEO-ZMC bank windows (MAME's ROM_RELOAD).
    const auto& m1 = regions_["audiocpu"];
    const size_t m1Len = m1.size() > 0x10000 ? m1.size() - 0x10000 : m1.size();
    z80Cart_.assign(0x10000 + m1Len, 0xff);
    std::copy(m1.begin(), m1.begin() + static_cast<std::ptrdiff_t>(std::min(m1Len, m1.size())), z80Cart_.begin());
    std::copy(m1.begin(), m1.begin() + static_cast<std::ptrdiff_t>(std::min(m1Len, m1.size())), z80Cart_.begin() + 0x10000);

    // Sprite graphics: C ROM pairs to one byte per pixel (MAME's layout).
    const auto& c = regions_["sprites"];
    std::uint32_t mask = 0xffffffff;
    for (std::uint32_t b = 0x80000000; b; b >>= 1) {
        if ((static_cast<std::uint32_t>(c.size()) * 2 - 1) & b) break;
        mask >>= 1;
    }
    spriteMask_ = mask;
    sprites8_.assign(static_cast<size_t>(mask) + 1, 0);
    std::uint8_t* d = sprites8_.data();
    for (size_t i = 0; i + 0x80 <= c.size(); i += 0x80) {
        const std::uint8_t* s = &c[i];
        for (int y = 0; y < 16; ++y) {
            for (int half : {0x40, 0x00})
                for (int x = 0; x < 8; ++x)
                    *d++ = static_cast<std::uint8_t>(bit(s[half | 3 | (y << 2)], x) << 3 | bit(s[half | 1 | (y << 2)], x) << 2 |
                                                     bit(s[half | 2 | (y << 2)], x) << 1 | bit(s[half | 0 | (y << 2)], x));
        }
    }
    regions_["sprites"].clear();
    regions_["sprites"].shrink_to_fit();

    vram_.assign(0x8000 + 0x800, 0);
    frame_.assign(static_cast<size_t>(kWidth * kHeight), 0);
    cpu_ = std::make_unique<Cpu68k>();
    cpu_->m = this;
    cpu_->setModel(moira::CPUModel::M68000);
    z80_ = std::make_unique<::z80>();
    ym_ = std::make_unique<Ym>();
    ym_->a = adpcmA_;
    ym_->b = adpcmB_;
    ym_->chip.set_fidelity(ymfm::OPN_FIDELITY_MIN);
    reset();
}

NeoGeoMachine::~NeoGeoMachine() = default;

emu::VideoInfo NeoGeoMachine::video() const {
    return emu::VideoInfo{kWidth, kHeight, 4.0 / 3.0, info_.rotation, kPixelClock / (kHTotal * kVTotal)};
}

int NeoGeoMachine::audioSampleRate() const { return static_cast<int>(std::lround(kYmClock / kYmPrescale)); }

void NeoGeoMachine::reset() {
    ram_.fill(0);
    std::fill(vram_.begin(), vram_.end(), 0);
    palette_.fill(0);
    pens_.fill(0);
    bankBase_ = prog_->size() > 0x100000 ? 0x100000 : 0;
    vramAddr_ = vramModulo_ = vramRead_ = 0;
    animSpeed_ = animCounter_ = animFrame_ = 0;
    animDisabled_ = false;
    timerControl_ = 0;
    displayCounter_ = 0;
    timerAt_ = -1;
    irqVblank_ = irqRaster_ = false;
    irqCold_ = true;  // the MVS starts with a pending IRQ3
    shadow_ = cartVectors_ = cartAudio_ = sramUnlocked_ = false;
    paletteBank_ = 0;
    line_ = 0;

    z80_init(z80_.get());
    z80_->userdata = this;
    z80_->read_byte = [](void* u, std::uint16_t a) { return static_cast<NeoGeoMachine*>(u)->z80Read(a); };
    z80_->write_byte = [](void* u, std::uint16_t a, std::uint8_t v) { static_cast<NeoGeoMachine*>(u)->z80Write(a, v); };
    z80_->port_in = [](::z80* z, std::uint8_t p) -> std::uint8_t {
        return static_cast<NeoGeoMachine*>(z->userdata)->z80In(static_cast<std::uint16_t>(z->port_hi << 8 | p));
    };
    z80_->port_out = [](::z80* z, std::uint8_t p, std::uint8_t v) {
        static_cast<NeoGeoMachine*>(z->userdata)->z80Out(static_cast<std::uint16_t>(z->port_hi << 8 | p), v);
    };
    z80Bank_ = {0x1e, 0x0e, 0x06, 0x02};  // MAME's initial banks for unbanked carts
    z80Ram_.fill(0);
    soundLatch_ = soundReply_ = 0;
    latchPending_ = nmiEnabled_ = nmiLine_ = false;
    z80Cycle_ = 0;
    ym_->chip.reset();
    ym_->timer[0] = ym_->timer[1] = -1;
    audio_.clear();
    sampleAcc_ = 0;

    cpu_->reset();
    frameStart_ = cpu_->getClock();
    updateIpl();
}

void NeoGeoMachine::setInput(const emu::CabinetInput& in) {
    auto pad = [](const emu::PlayerInput& p) {
        std::uint8_t v = 0xff;
        if (p.up) v &= ~0x01;
        if (p.down) v &= ~0x02;
        if (p.left) v &= ~0x04;
        if (p.right) v &= ~0x08;
        for (int b = 0; b < 4; ++b)
            if (p.button[static_cast<size_t>(b)]) v &= static_cast<std::uint8_t>(~(0x10 << b));
        return v;
    };
    p1_ = pad(in.player[0]);
    p2_ = pad(in.player[1]);
    std::uint8_t c = 0xff;
    if (in.player[0].coin) c &= ~0x01;
    if (in.player[1].coin) c &= ~0x02;
    if (in.service) c &= ~0x04;
    coins_ = c;
    std::uint8_t s = 0x0f;  // P1 start, P1 select, P2 start, P2 select (active low)
    if (in.player[0].start) s &= ~0x01;
    if (in.player[1].start) s &= ~0x04;
    startSel_ = s;
}

std::int64_t NeoGeoMachine::clock() const { return cpu_->getClock(); }

int NeoGeoMachine::ipl() const { return irqCold_ ? 3 : irqRaster_ ? 2 : irqVblank_ ? 1 : 0; }
void NeoGeoMachine::updateIpl() { cpu_->setIPL(static_cast<moira::u8>(ipl())); }

// ------------------------------------------------------------------ 68000 map

std::uint16_t NeoGeoMachine::read16(std::uint32_t a) {
    a &= 0xfffffe;
    auto word = [](const std::vector<std::uint8_t>& r, std::uint32_t o) -> std::uint16_t {
        return o + 1 < r.size() ? static_cast<std::uint16_t>(r[o] << 8 | r[o + 1]) : 0xffff;
    };
    if (a < 0x80) return cartVectors_ ? word(*prog_, a) : word(*bios_, a);
    if (a < 0x100000) return word(*prog_, a);
    if (a < 0x200000) return static_cast<std::uint16_t>(ram_[a & 0xffff] << 8 | ram_[(a & 0xffff) + 1]);
    if (a < 0x300000) return word(*prog_, bankBase_ + (a & 0xfffff));
    if (a < 0x320000) {
        if (a & 0x80) return 0xff80;  // TEST: service switch off, JAMMA sense
        return static_cast<std::uint16_t>(p1_ << 8 | 0xff);  // P1 + DIP switches (all off)
    }
    if (a < 0x340000) {
        std::uint8_t lo = static_cast<std::uint8_t>(coins_ & 0x1f);
        lo = static_cast<std::uint8_t>(lo | (rtc_.tp(seconds_) ? 0x40 : 0) | (rtc_.dataOut(seconds_) ? 0x80 : 0));
        return static_cast<std::uint16_t>(soundReply_ << 8 | lo);
    }
    if (a < 0x360000) return static_cast<std::uint16_t>(p2_ << 8 | 0xff);
    if (a < 0x380000) return 0xffff;
    if (a < 0x3a0000) {
        // Start/select, no memory card (bits 12-14 set), MVS hardware (bit 15).
        return static_cast<std::uint16_t>(0x8000 | 0x7000 | (startSel_ & 0x0f) << 8 | 0xff);
    }
    if (a < 0x3c0000) return 0xffff;
    if (a < 0x400000) return videoRead(static_cast<int>((a >> 1) & 7));
    if (a < 0x800000) return palette_[static_cast<size_t>(paletteBank_ * 0x1000 + ((a & 0x1fff) >> 1))];
    if (a < 0xc00000) return 0xffff;  // memory card slot (empty)
    if (a < 0xd00000) return bios_->empty() ? 0xffff : word(*bios_, a % static_cast<std::uint32_t>(bios_->size()));
    if (a < 0xe00000) return static_cast<std::uint16_t>(sram_[a & 0xffff] << 8 | sram_[(a & 0xffff) + 1]);
    return 0xffff;
}

std::uint8_t NeoGeoMachine::read8(std::uint32_t a) {
    const std::uint16_t w = read16(a & ~1u);
    return static_cast<std::uint8_t>((a & 1) ? w : w >> 8);
}

void NeoGeoMachine::write16(std::uint32_t a, std::uint16_t v) {
    a &= 0xfffffe;
    if (a >= 0x100000 && a < 0x200000) {
        ram_[a & 0xffff] = static_cast<std::uint8_t>(v >> 8);
        ram_[(a & 0xffff) + 1] = static_cast<std::uint8_t>(v);
        return;
    }
    if (a >= 0x2ffff0 && a < 0x300000) {  // cartridge bank select
        const std::uint32_t bank = v & 7;
        const std::uint32_t base = (bank + 1) * 0x100000;
        bankBase_ = prog_->size() > 0x100000 && base < prog_->size() ? base : (prog_->size() > 0x100000 ? 0x100000 : 0);
        return;
    }
    if (a >= 0x320000 && a < 0x340000) { soundCommand(static_cast<std::uint8_t>(v >> 8)); return; }
    if (a >= 0x380000 && a < 0x3c0000) { write8(a + 1, static_cast<std::uint8_t>(v)); return; }
    if (a >= 0x3c0000 && a < 0x400000) { videoWrite(static_cast<int>((a >> 1) & 7), v); return; }
    if (a >= 0x400000 && a < 0x800000) { writePalette(paletteBank_ * 0x1000 + static_cast<int>((a & 0x1fff) >> 1), v); return; }
    if (a >= 0xd00000 && a < 0xe00000) {
        if (sramUnlocked_) {
            sram_[a & 0xffff] = static_cast<std::uint8_t>(v >> 8);
            sram_[(a & 0xffff) + 1] = static_cast<std::uint8_t>(v);
        }
        return;
    }
    // 0x300000: watchdog; everything else is ROM or unmapped.
}

void NeoGeoMachine::write8(std::uint32_t a, std::uint8_t v) {
    a &= 0xffffff;
    if (a >= 0x100000 && a < 0x200000) { ram_[a & 0xffff] = v; return; }
    if (a >= 0x320000 && a < 0x340000) {
        if (!(a & 1)) soundCommand(v);
        return;
    }
    if (a >= 0x380000 && a < 0x3a0000) {
        if (!(a & 1)) return;
        const int offset = static_cast<int>((a & 0xff) >> 1);
        if ((offset & 0x78) == 0x28) rtc_.write(v & 1, v & 2, v & 4);  // calendar: data, clock, strobe
        return;  // 0x00: controller select, 0x08: memory card bank, 0x30: coin counters
    }
    if (a >= 0x3a0000 && a < 0x3c0000) {
        if (!(a & 1)) return;
        const int offset = static_cast<int>((a & 0x1f) >> 1);
        systemLatch(offset & 7, (offset >> 3) & 1);
        return;
    }
    if (a >= 0x3c0000 && a < 0x400000) {
        // Byte writes: the upper byte is written to both halves; the lower is ignored.
        if (!(a & 1)) videoWrite(static_cast<int>((a >> 1) & 7), static_cast<std::uint16_t>(v << 8 | v));
        return;
    }
    if (a >= 0x400000 && a < 0x800000) {
        const int i = paletteBank_ * 0x1000 + static_cast<int>((a & 0x1fff) >> 1);
        std::uint16_t w = palette_[static_cast<size_t>(i)];
        w = (a & 1) ? static_cast<std::uint16_t>((w & 0xff00) | v) : static_cast<std::uint16_t>((w & 0x00ff) | v << 8);
        writePalette(i, w);
        return;
    }
    if (a >= 0xd00000 && a < 0xe00000) {
        if (sramUnlocked_) sram_[a & 0xffff] = v;
        return;
    }
}

void NeoGeoMachine::systemLatch(int b, bool value) {
    switch (b) {
    case 0: shadow_ = value; break;
    case 1: cartVectors_ = value; break;
    case 5: cartAudio_ = value; break;  // also selects the cartridge fix layer
    case 6: sramUnlocked_ = value; break;
    case 7: paletteBank_ = value ? 1 : 0; break;
    default: break;  // 2-4: memory card control
    }
}

void NeoGeoMachine::writePalette(int index, std::uint16_t v) {
    palette_[static_cast<size_t>(index)] = v;
    const int dark = v >> 15;
    const int r = ((v >> 14) & 1) | ((v >> 7) & 0x1e);
    const int g = ((v >> 13) & 1) | ((v >> 3) & 0x1e);
    const int b = ((v >> 12) & 1) | ((v << 1) & 0x1e);
    // 5-bit resistor DAC; the "dark" bit pulls every channel down slightly,
    // the shadow mode about halves the brightness.
    auto level = [&](int c, bool shadow) {
        double x = c * 255.0 / 31.0;
        if (dark) x *= 0.94;
        if (shadow) x *= 0.5;
        return static_cast<std::uint32_t>(std::lround(x));
    };
    pens_[static_cast<size_t>(index)] = level(r, false) << 16 | level(g, false) << 8 | level(b, false);
    pens_[static_cast<size_t>(index + 0x2000)] = level(r, true) << 16 | level(g, true) << 8 | level(b, true);
}

// ------------------------------------------------------------------ LSPC

std::uint16_t NeoGeoMachine::videoRead(int reg) {
    switch (reg) {
    case 0: case 1: return vramRead_;
    case 2: return vramModulo_;
    default: {
        int v = line_ + 0x100;
        if (v >= 0x200) v -= kVTotal;
        return static_cast<std::uint16_t>(v << 7 | (animCounter_ & 7));
    }
    }
}

void NeoGeoMachine::armTimerRelative(std::int64_t from) {
    timerAt_ = from + static_cast<std::int64_t>(displayCounter_ + 1ull) * kCpuPerPixel;
}

void NeoGeoMachine::videoWrite(int reg, std::uint16_t v) {
    switch (reg) {
    case 0:
        vramAddr_ = (v & 0x8000) ? (v & 0x87ff) : v;
        vramRead_ = vram_[vramAddr_];
        break;
    case 1:
        vram_[vramAddr_] = v;
        vramAddr_ = static_cast<std::uint16_t>((vramAddr_ & 0x8000) | ((vramAddr_ + vramModulo_) & 0x7fff));
        if (vramAddr_ & 0x8000) vramAddr_ &= 0x87ff;
        vramRead_ = vram_[vramAddr_];
        break;
    case 2: vramModulo_ = v; break;
    case 3:
        animSpeed_ = static_cast<std::uint8_t>(v >> 8);
        animDisabled_ = v & 8;
        timerControl_ = v & 0xf0;
        break;
    case 4: displayCounter_ = (displayCounter_ & 0xffff) | static_cast<std::uint32_t>(v) << 16; break;
    case 5:
        displayCounter_ = (displayCounter_ & 0xffff0000) | v;
        if (bit(timerControl_, 5)) armTimerRelative(clock());  // load relative
        break;
    case 6:  // acknowledge
        if (v & 1) irqCold_ = false;
        if (v & 2) irqRaster_ = false;
        if (v & 4) irqVblank_ = false;
        updateIpl();
        break;
    default: break;
    }
}

void NeoGeoMachine::renderLine(int line) {
    if (line < kVisibleTop || line >= kVBlankStart) return;
    const int pal = paletteBank_ * 0x1000 + (shadow_ ? 0x2000 : 0);
    const std::uint32_t* pens = &pens_[static_cast<size_t>(pal)];
    std::uint32_t* row = &frame_[static_cast<size_t>((line - kVisibleTop) * kWidth)];
    std::fill(row, row + kWidth, pens[0xfff]);

    // Sprites: the first 96 found on this line, in sprite number order.
    int y = 0, x = 0, rows = 0, zoomY = 0, zoomX = 0, active = 0;
    for (int n = 0; n < 381 && active < 96; ++n) {
        const std::uint16_t yc = vram_[0x8200 | static_cast<size_t>(n)];
        const std::uint16_t zc = vram_[0x8000 | static_cast<size_t>(n)];
        if (yc & 0x40) {  // chained to the previous sprite
            x = (x + zoomX + 1) & 0x1ff;
            zoomX = (zc >> 8) & 0x0f;
        } else {
            y = 0x200 - (yc >> 7);
            x = vram_[0x8400 | static_cast<size_t>(n)] >> 7;
            zoomY = zc & 0xff;
            zoomX = (zc >> 8) & 0x0f;
            rows = yc & 0x3f;
        }
        if (rows == 0) continue;
        const bool on = rows >= 0x20 || ((line - y) & 0x1ff) < rows * 0x10;
        if (!on) continue;
        ++active;
        if (x >= 0x140 && x <= 0x1f0) continue;  // entirely off screen

        const int spriteLine = (line - y) & 0x1ff;
        int zoomLine = spriteLine & 0xff;
        bool invert = spriteLine & 0x100;
        if (invert) zoomLine ^= 0xff;
        if (rows > 0x20) {
            zoomLine = zoomLine % ((zoomY + 1) << 1);
            if (zoomLine > zoomY) {
                zoomLine = ((zoomY + 1) << 1) - 1 - zoomLine;
                invert = !invert;
            }
        }
        const std::uint8_t yt = zoomY_->size() > 0xffff ? (*zoomY_)[static_cast<size_t>((zoomY << 8) | zoomLine)] : 0;
        int sy = yt & 0x0f, tile = yt >> 4;
        if (invert) {
            sy ^= 0x0f;
            tile ^= 0x1f;
        }
        const size_t ao = static_cast<size_t>(n << 6 | tile << 1);
        const std::uint16_t attr = vram_[ao + 1];
        std::uint32_t code = ((attr << 12) & 0xf0000u) | vram_[ao];
        if (!animDisabled_) {
            if (attr & 8) code = (code & ~7u) | (animCounter_ & 7);
            else if (attr & 4) code = (code & ~3u) | (animCounter_ & 3);
        }
        if (attr & 2) sy ^= 0x0f;
        std::uint16_t zx = kZoomX[zoomX];
        int gfx = static_cast<int>(((code << 8) | static_cast<std::uint32_t>(sy << 4)) & spriteMask_);
        int step = 1;
        if (attr & 1) {
            gfx += 0x0f;
            step = -1;
        }
        const std::uint32_t* lp = &pens[(attr >> 8) << 4];
        int px = x;
        for (int i = 0; i < 16; ++i) {
            if (zx & 0x8000) {
                const int sx = px >= 0x200 - 16 && x > 0x1f0 ? px - 0x200 : px;  // wrap-around at the left edge
                if (sx >= 0 && sx < kWidth) {
                    const std::uint8_t p = sprites8_[static_cast<size_t>(gfx) & spriteMask_];
                    if (p) row[sx] = lp[p];
                }
                ++px;
            }
            zx = static_cast<std::uint16_t>(zx << 1);
            if (!zx) break;
            gfx += step;
        }
    }

    // Fix layer: 40 columns of 8x8 tiles, on top of everything.
    const auto& fix = cartAudio_ && !fixCart_->empty() ? *fixCart_ : *fixBios_;
    if (fix.empty()) return;
    const size_t fixMask = fix.size() - 1;
    for (int col = 0; col < 40; ++col) {
        const std::uint16_t t = vram_[0x7000 | static_cast<size_t>(col * 0x20 + (line >> 3))];
        const int code = t & 0x0fff;
        const std::uint32_t* cp = &pens[(t >> 12) << 4];
        const size_t base = static_cast<size_t>((code << 5) | (line & 7));
        static const int kOffs[4] = {0x10, 0x18, 0x00, 0x08};
        for (int k = 0; k < 4; ++k) {
            const std::uint8_t dd = fix[(base + static_cast<size_t>(kOffs[k])) & fixMask];
            const int sx = col * 8 + k * 2;
            if (dd & 0x0f) row[sx] = cp[dd & 0x0f];
            if (dd & 0xf0) row[sx + 1] = cp[dd >> 4];
        }
    }
}

// ------------------------------------------------------------------ Z80 side

std::uint8_t NeoGeoMachine::z80Read(std::uint16_t a) {
    auto cart = [&](std::uint32_t o) { return o < z80Cart_.size() ? z80Cart_[o] : static_cast<std::uint8_t>(0xff); };
    if (a < 0x8000) {
        if (!cartAudio_ && !z80Bios_->empty()) return (*z80Bios_)[a % z80Bios_->size()];
        return cart(a);
    }
    if (a >= 0xf800) return z80Ram_[a & 0x7ff];
    // NEO-ZMC windows: 0x8000 (16K), 0xc000 (8K), 0xe000 (4K), 0xf000 (2K).
    const std::uint32_t len = static_cast<std::uint32_t>(z80Cart_.size());
    const std::uint32_t mask = (len > 0x10000 ? len - 0x10000 - 1 : 0) & 0x3ffff;
    int region;
    std::uint32_t off;
    if (a < 0xc000) { region = 3; off = a - 0x8000u; }
    else if (a < 0xe000) { region = 2; off = a - 0xc000u; }
    else if (a < 0xf000) { region = 1; off = a - 0xe000u; }
    else { region = 0; off = a - 0xf000u; }
    const std::uint32_t base = 0x10000 + ((z80Bank_[static_cast<size_t>(region)] << (11 + region)) & mask);
    return cart(base + off);
}

void NeoGeoMachine::z80Write(std::uint16_t a, std::uint8_t v) {
    if (a >= 0xf800) z80Ram_[a & 0x7ff] = v;
}

std::uint8_t NeoGeoMachine::z80In(std::uint16_t port) {
    const int lo = port & 0xff;
    if ((lo & 0x0f) == 0x00 && lo < 0x10) {  // sound command (reading acknowledges it)
        latchPending_ = false;
        updateNmi();
        return soundLatch_;
    }
    if ((lo & 0xfc) == 0x04) return ym_->chip.read(static_cast<std::uint32_t>(lo & 3));
    if ((lo & 0x0c) == 0x08) {  // bank select via the address's upper byte
        z80Bank_[static_cast<size_t>(lo & 3)] = static_cast<std::uint32_t>(port >> 8);
        return 0;
    }
    return 0xff;
}

void NeoGeoMachine::z80Out(std::uint16_t port, std::uint8_t v) {
    const int lo = port & 0xff;
    if (lo == 0x00) {
        latchPending_ = false;
        updateNmi();
    } else if ((lo & 0xfc) == 0x04) {
        ym_->chip.write(static_cast<std::uint32_t>(lo & 3), v);
    } else if ((lo & 0xef) == 0x08) {  // 0x08 enables, 0x18 disables the NMI
        nmiEnabled_ = !(lo & 0x10);
        updateNmi();
    } else if (lo == 0x0c) {
        soundReply_ = v;
    }
}

void NeoGeoMachine::soundCommand(std::uint8_t v) {
    // Let the sound CPU catch up first so commands are not lost.
    runZ80Until(static_cast<double>(clock() - frameStart_) * (kZ80Clock / (kPixelClock * kCpuPerPixel)));
    soundLatch_ = v;
    latchPending_ = true;
    updateNmi();
}

void NeoGeoMachine::updateNmi() {
    const bool line = latchPending_ && nmiEnabled_;
    if (line && !nmiLine_) z80_gen_nmi(z80_.get());
    nmiLine_ = line;
}

void NeoGeoMachine::runZ80Until(double cycle) {
    ::z80* z = z80_.get();
    while (z80Cycle_ < cycle) {
        if (ym_->irq && !z->int_pending) z80_gen_int(z, 0xff);
        const unsigned long before = z->cyc;
        z80_step(z);
        z80Cycle_ += static_cast<double>(z->cyc - before);
    }
}

void NeoGeoMachine::generateAudio(int samples) {
    for (int i = 0; i < samples; ++i) {
        ym_->advance(kYmPrescale);
        ymfm::ym2610::output_data out;
        ym_->chip.generate(&out, 1);
        const double ssg = out.data[2] * 0.84;
        const auto l = static_cast<std::int16_t>(std::clamp(out.data[0] * 0.98 + ssg, -32768.0, 32767.0));
        const auto r = static_cast<std::int16_t>(std::clamp(out.data[1] * 0.98 + ssg, -32768.0, 32767.0));
        audio_.push_back(l);
        audio_.push_back(r);
    }
}

// ------------------------------------------------------------------ frame

void NeoGeoMachine::runFrame() {
    frameStart_ = clock();
    z80Cycle_ = 0;
    const double z80PerCpu = kZ80Clock / (kPixelClock * kCpuPerPixel);
    const double samplesPerLine = (kHTotal / kPixelClock) * (kYmClock / kYmPrescale);
    for (line_ = 0; line_ < kVTotal; ++line_) {
        const std::int64_t lineStart = frameStart_ + static_cast<std::int64_t>(line_) * kCpuPerLine;
        const std::int64_t lineEnd = lineStart + kCpuPerLine;
        std::int64_t vblankAt = -1, reloadAt = -1;
        if (line_ == kVBlankStart) {
            vblankAt = lineStart + kVblankIrqPixel * kCpuPerPixel;
            reloadAt = lineStart + kReloadPixel * kCpuPerPixel;
        }
        if (line_ == kAutoAnimLine) {
            if (animFrame_ == 0) {
                animFrame_ = animSpeed_;
                ++animCounter_;
            } else {
                --animFrame_;
            }
        }
        while (clock() < lineEnd) {
            std::int64_t next = lineEnd;
            for (std::int64_t e : {timerAt_, vblankAt, reloadAt})
                if (e >= 0 && e > clock() && e < next) next = e;
            cpu_->executeUntil(next);
            const std::int64_t now = clock();
            if (vblankAt >= 0 && now >= vblankAt) {
                irqVblank_ = true;
                vblankAt = -1;
                updateIpl();
            }
            if (reloadAt >= 0 && now >= reloadAt) {
                if (bit(timerControl_, 6)) armTimerRelative(reloadAt);  // reload at VBLANK
                reloadAt = -1;
            }
            if (timerAt_ >= 0 && now >= timerAt_) {
                const std::int64_t fired = timerAt_;
                timerAt_ = -1;
                if (bit(timerControl_, 4)) {
                    irqRaster_ = true;
                    updateIpl();
                }
                if (bit(timerControl_, 7)) armTimerRelative(fired);  // repeat
            }
        }
        runZ80Until(static_cast<double>(lineEnd - frameStart_) * z80PerCpu);
        sampleAcc_ += samplesPerLine;
        const int n = static_cast<int>(sampleAcc_);
        sampleAcc_ -= n;
        generateAudio(n);
        renderLine(line_);
    }
    seconds_ += kHTotal * kVTotal / kPixelClock;
    line_ = 0;
}

void NeoGeoMachine::takeAudio(std::vector<std::int16_t>& out) {
    out.insert(out.end(), audio_.begin(), audio_.end());
    audio_.clear();
}

std::vector<std::uint8_t> NeoGeoMachine::nvram() const { return {sram_.begin(), sram_.end()}; }

void NeoGeoMachine::setNvram(const std::vector<std::uint8_t>& data) {
    if (data.size() == sram_.size()) std::copy(data.begin(), data.end(), sram_.begin());
}

// ------------------------------------------------------------------ games

namespace {

using emu::RomLoad;

struct Def {
    emu::GameInfo info;
    std::vector<std::pair<std::string, std::uint32_t>> regionSizes;
    std::vector<emu::RomFile> roms;
};

// MVS BIOS chips, any one of which is enough (searched in this order).
// Names and CRCs from MAME's neogeo.zip definition.
const emu::RomFile kBios[] = {
    {"bios", "sp-s2.sp1", 0x9036d879, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "sp-s.sp1", 0xc7f2fa45, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "sp-u2.sp1", 0xe72943de, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "sp-e.sp1", 0x2723a5b5, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "sp1-u2", 0x62f021f4, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "vs-bios.rom", 0xf0e8f27d, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "sp-j2.sp1", 0xacede59c, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "sp-s3.sp1", 0x91b64be3, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "sp-45.sp1", 0x03cc9f6a, 0, 0x80000, RomLoad::WordSwap16},
    {"bios", "uni-bios_4_0.rom", 0xa7aab458, 0, 0x20000, RomLoad::WordSwap16},
    {"bios", "uni-bios_3_3.rom", 0x24858466, 0, 0x20000, RomLoad::WordSwap16},
};

const std::vector<Def>& defs() {
    static const std::vector<Def> d = [] {
        std::vector<Def> v;
#include "neogeo_games_data.inc"
        for (auto& g : v) {
            g.info.system = "SNK Neo Geo MVS";
            g.info.players = 2;
            g.info.buttons = static_cast<int>(g.info.buttonNames.size());
            g.info.joystick = "8-way";
            g.info.bios = "neogeo";
        }
        return v;
    }();
    return d;
}

const Def* findDef(const std::string& set) {
    for (const auto& g : defs())
        if (g.info.set == set) return &g;
    return nullptr;
}

}  // namespace

std::vector<emu::GameInfo> moduleGames() {
    std::vector<emu::GameInfo> v;
    for (const auto& g : defs()) v.push_back(g.info);
    return v;
}

std::vector<emu::RomFile> moduleRomFiles(const std::string& set) {
    const auto* g = findDef(set);
    if (!g) return {};
    return g->roms;
}

std::vector<std::string> moduleMissing(const std::string& set, const std::filesystem::path& gamesDir) {
    const auto* g = findDef(set);
    if (!g) return {set + ".zip"};
    auto missing = emu::checkRomSet(gamesDir, set, "neogeo", g->roms);
    bool bios = false;
    for (const auto& b : kBios) bios = bios || emu::checkRomSet(gamesDir, set, "neogeo", {b}).empty();
    if (!bios) missing.push_back("an MVS BIOS in neogeo.zip (for example sp-s2.sp1)");
    return missing;
}

std::unique_ptr<emu::Machine> moduleCreate(const std::string& set, const std::filesystem::path& gamesDir,
                                           std::string* error) {
    const auto* g = findDef(set);
    if (!g) return nullptr;
    std::vector<emu::RomRegion> regions;
    for (const auto& [name, size] : g->regionSizes) regions.push_back({name.c_str(), size, 0});
    regions.push_back({"bios", 0x80000, 0xff});

    if (!emu::findSetZip(gamesDir, set)) {
        if (error) *error = set + ".zip not found in " + gamesDir.string();
        return nullptr;
    }
    // Use the first MVS BIOS present in neogeo.zip.
    for (const auto& bios : kBios) {
        auto files = g->roms;
        files.push_back(bios);
        if (!emu::checkRomSet(gamesDir, set, "neogeo", {bios}).empty()) continue;
        auto loaded = emu::loadRomSet(gamesDir, set, "neogeo", regions, files);
        if (!loaded.ok) {
            if (error) {
                *error = "ROM set '" + set + "' (with neogeo.zip) is incomplete or a different version. Missing or wrong:";
                for (const auto& m : loaded.missing) *error += "\n  " + m;
            }
            return nullptr;
        }
        auto& b = loaded.regions["bios"];
        b.resize(bios.length);
        return std::make_unique<NeoGeoMachine>(g->info, std::move(loaded.regions));
    }
    if (error) *error = "No Neo Geo MVS BIOS found: put neogeo.zip (current MAME version) in the Games or BIOS folder";
    return nullptr;
}

}  // namespace zen1::neogeo
