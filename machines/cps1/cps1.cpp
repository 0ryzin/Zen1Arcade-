// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom CPS1 machine for the Zen1 engine: CPUs, memory map, sound board,
// inputs and frame loop. Hardware behaviour ported from MAME's CPS1 driver
// (BSD-3-Clause, copyright-holders: Paul Leaman); see cps1.hpp.
#include "cps1.hpp"
#include "cps1_cpu.hpp"

#include <Moira.h>
#include <ymfm_opm.h>

extern "C" {
#include <z80.h>
}

#include <algorithm>
#include <cmath>

namespace zen1::cps1 {

namespace {
constexpr double kPixelClock = 8'000'000.0;  // 16 MHz / 2
constexpr int kHTotal = 512, kVTotal = 262, kVBlankStart = 240;
constexpr double kZ80Clock = 3'579'545.0;
constexpr double kYmClock = 3'579'545.0;
constexpr double kOkiClock = 1'000'000.0;    // 16 MHz / 4 / 4
constexpr double kLineSeconds = kHTotal / kPixelClock;
}  // namespace

// --------------------------------------------------------------- YM2151 ---

struct Cps1Machine::Ym : ymfm::ymfm_interface {
    ymfm::ym2151 chip{*this};
    std::int32_t timer[2] = {-1, -1};  // remaining input clocks, -1 = stopped
    bool irq = false;

    void ymfm_set_timer(std::uint32_t t, std::int32_t clocks) override { timer[t & 1] = clocks; }
    void ymfm_update_irq(bool asserted) override { irq = asserted; }
    void advance(std::int32_t clocks) {
        for (std::uint32_t t = 0; t < 2; ++t) {
            if (timer[t] < 0) continue;
            timer[t] -= clocks;
            if (timer[t] <= 0) {
                timer[t] = -1;
                m_engine->engine_timer_expired(t);  // may re-arm the timer
            }
        }
    }
};

// -------------------------------------------------------------- machine ---

Cps1Machine::Cps1Machine(const GameDef& def, emu::RegionMap regions)
    : def_(def), regions_(std::move(regions)) {
    mainRom_ = &regions_["maincpu"];
    audioRom_ = &regions_["audiocpu"];
    gfxRom_ = &regions_["gfx"];
    oki_.setRom(&regions_["oki"]);
    cps2_ = def.cps2;
    if (cps2_) {
        // CPS-2 graphics ROMs are interleaved in 2 MB banks (MAME "unshuffle").
        auto& g = *gfxRom_;
        const size_t bank = 0x200000;
        std::vector<std::uint8_t> tmp(8);
        auto unshuffle = [&](auto&& self, size_t base, size_t len) -> void {  // len in 8-byte units
            if (len == 2) return;
            len /= 2;
            self(self, base, len);
            self(self, base + len * 8, len);
            for (size_t i = 0; i < len / 2; ++i) {
                std::uint8_t* a = &g[base + (len / 2 + i) * 8];
                std::uint8_t* b = &g[base + (len + i) * 8];
                std::swap_ranges(a, a + 8, b);
            }
        };
        for (size_t i = 0; i + bank <= g.size(); i += bank) unshuffle(unshuffle, i, bank / 8);
        qsound_.setSamples(&regions_["qsound"]);
        decryptProgram();
    }
    decodeGfx();

    cpu_ = std::make_unique<Cpu68k>();
    cpu_->m = this;
    cpu_->setModel(moira::CPUModel::M68000);

    z80_ = std::make_unique<::z80>();
    ym_ = std::make_unique<Ym>();
    reset();
}

Cps1Machine::~Cps1Machine() = default;

emu::VideoInfo Cps1Machine::video() const {
    return emu::VideoInfo{kWidth, kHeight, 4.0 / 3.0, def_.info.rotation,
                          kPixelClock / (kHTotal * kVTotal)};
}

int Cps1Machine::audioSampleRate() const {
    return cps2_ ? static_cast<int>(std::lround(QSound::kSampleRate)) : static_cast<int>(std::lround(kYmClock / 64.0));
}

void Cps1Machine::reset() {
    std::fill(mainRam_.begin(), mainRam_.end(), 0);
    std::fill(gfxram_.begin(), gfxram_.end(), 0);
    cpsA_.fill(0);
    cpsB_.fill(0);
    cpsA_[0x00] = 0x9200;  // object base
    cpsA_[0x01] = 0x9000;  // scroll1 base
    cpsA_[0x02] = 0x9040;  // scroll2 base
    cpsA_[0x03] = 0x9080;  // scroll3 base
    cpsA_[0x04] = 0x9100;  // other (row scroll) base
    palette_.fill(0);
    irqPending_ = false;

    z80_init(z80_.get());
    z80_->userdata = this;
    if (cps2_) {
        z80_->read_byte = [](void* u, std::uint16_t a) { return static_cast<Cps1Machine*>(u)->z80ReadCps2(a); };
        z80_->write_byte = [](void* u, std::uint16_t a, std::uint8_t v) { static_cast<Cps1Machine*>(u)->z80WriteCps2(a, v); };
    } else {
        z80_->read_byte = [](void* u, std::uint16_t a) { return static_cast<Cps1Machine*>(u)->z80Read(a); };
        z80_->write_byte = [](void* u, std::uint16_t a, std::uint8_t v) { static_cast<Cps1Machine*>(u)->z80Write(a, v); };
    }
    z80_->port_in = [](::z80*, std::uint8_t) -> std::uint8_t { return 0xff; };
    z80_->port_out = [](::z80*, std::uint8_t, std::uint8_t) {};
    z80Cycle_ = 0.0;
    audioBank_ = 0;
    soundLatch_ = soundLatch2_ = 0;
    ym_->chip.reset();
    ym_->timer[0] = ym_->timer[1] = -1;
    oki_.reset();
    oki_.setPin7(true);
    okiPhase_ = 0.0;
    audio_.clear();
    output_.fill(0);
    for (auto& o : objram_) std::fill(o.begin(), o.end(), 0);
    std::fill(bufferedObj2_.begin(), bufferedObj2_.end(), 0);
    for (auto& q : qram_) std::fill(q.begin(), q.end(), 0);
    std::fill(extraRam_.begin(), extraRam_.end(), 0);
    objBank_ = 0;
    priCtrl_ = 0;
    qsound_.reset();
    qsAcc_ = z80IrqAcc_ = 0.0;
    z80Held_ = false;
    for (int i = 0; i < 3; ++i) rasterCounter_[i] = rasterReload_[i] = 0;
    renderedRows_ = 0;
    ipl_ = 0;

    cpu_->reset();
}

void Cps1Machine::setInput(const emu::CabinetInput& in) {
    if (cps2_) {
        // IN0: P1 / P2 stick and buttons 1-4. IN1: buttons 4-6 on six-button
        // games (P1 bits 0-2, P2 bits 4-5; P2 button 6 is IN2 bit 14).
        // IN2: EEPROM data (bit 0), test, service, starts, coins.
        std::uint16_t i0 = 0xffff, i1 = 0xffff, i2 = 0xffff;
        const int nb = def_.info.buttons;
        for (int p = 0; p < 2; ++p) {
            const auto& pl = in.player[static_cast<size_t>(p)];
            std::uint16_t bits = 0;
            if (pl.right && !pl.left) bits |= 0x01;
            if (pl.left && !pl.right) bits |= 0x02;
            if (pl.down && !pl.up) bits |= 0x04;
            if (pl.up && !pl.down) bits |= 0x08;
            for (int b = 0; b < std::min(nb, 4); ++b) {
                if (nb >= 6 && b == 3) break;  // six-button games read buttons 4-6 from IN1
                if (pl.button[static_cast<size_t>(b)]) bits |= static_cast<std::uint16_t>(0x10 << b);
            }
            i0 &= static_cast<std::uint16_t>(~(bits << (p * 8)));
            if (nb >= 6 || def_.info.set == "cybots" || def_.info.parent == "cybots") {
                const int extra = nb >= 6 ? 3 : 1;
                for (int b = 0; b < extra; ++b) {
                    if (!pl.button[static_cast<size_t>(3 + b)]) continue;
                    if (p == 1 && b == 2) i2 &= static_cast<std::uint16_t>(~0x4000);
                    else i1 &= static_cast<std::uint16_t>(~(1 << (b + p * 4)));
                }
            }
        }
        if (in.service) i2 &= static_cast<std::uint16_t>(~0x0004);
        if (in.player[0].start) i2 &= static_cast<std::uint16_t>(~0x0100);
        if (in.player[1].start) i2 &= static_cast<std::uint16_t>(~0x0200);
        if (in.player[0].coin) i2 &= static_cast<std::uint16_t>(~0x1000);
        if (in.player[1].coin) i2 &= static_cast<std::uint16_t>(~0x2000);
        cps2In0_ = i0;
        cps2In1_ = i1;
        cps2In2_ = i2;
        return;
    }
    std::uint8_t i0 = 0xff;
    if (in.player[0].coin) i0 &= ~0x01;
    if (in.player[1].coin) i0 &= ~0x02;
    if (in.service) i0 &= ~0x04;
    if (in.player[0].start) i0 &= ~0x10;
    if (in.player[1].start) i0 &= ~0x20;
    in0_ = i0;

    std::uint16_t i1 = 0xffff;
    std::uint8_t i2 = 0xff;
    for (int p = 0; p < 2; ++p) {
        const auto& pl = in.player[static_cast<size_t>(p)];
        const int s = p * 8;
        std::uint16_t bits = 0;
        // Opposite directions cancel, as on a real 8-way stick.
        if (pl.right && !pl.left) bits |= 0x01;
        if (pl.left && !pl.right) bits |= 0x02;
        if (pl.down && !pl.up) bits |= 0x04;
        if (pl.up && !pl.down) bits |= 0x08;
        for (int b = 0; b < 3 && b < def_.info.buttons; ++b)
            if (pl.button[static_cast<size_t>(b)]) bits |= static_cast<std::uint16_t>(0x10 << b);
        i1 &= static_cast<std::uint16_t>(~(bits << s));
        if (def_.info.buttons > 3)
            for (int b = 0; b < 3; ++b)
                if (pl.button[static_cast<size_t>(b + 3)]) i2 &= static_cast<std::uint8_t>(~(1 << (b + p * 4)));
    }
    in1_ = i1;
    in2_ = i2;
}

// ---- main CPU memory map ----

std::uint16_t Cps1Machine::read16(std::uint32_t a) {
    a &= ~1u;
    if (cps2_) return read16Cps2(a);
    if (a < 0x400000) return static_cast<std::uint16_t>(((*mainRom_)[a] << 8) | (*mainRom_)[a + 1]);
    if (a >= 0xff0000) return mainRam_[(a & 0xffff) >> 1];
    if (a >= 0x900000 && a < 0x930000) return gfxram_[(a - 0x900000) >> 1];
    if (a >= 0x800000 && a < 0x800008) return in1_;
    if (a >= 0x800018 && a < 0x800020) {
        const int off = static_cast<int>((a - 0x800018) >> 1);
        const std::uint8_t v = off == 0 ? in0_ : def_.dips[static_cast<size_t>(off - 1)];
        return static_cast<std::uint16_t>((v << 8) | 0xff);
    }
    if (a >= 0x800140 && a < 0x800180) return cpsBRead(static_cast<int>((a - 0x800140) >> 1));
    return 0xffff;
}

std::uint8_t Cps1Machine::read8(std::uint32_t a) {
    const std::uint16_t w = read16(a);
    return static_cast<std::uint8_t>((a & 1) ? (w & 0xff) : (w >> 8));
}

void Cps1Machine::write8(std::uint32_t a, std::uint8_t v) {
    if (a & 1) write16(a & ~1u, v, 0x00ff);
    else write16(a, static_cast<std::uint16_t>(v << 8), 0xff00);
}

void Cps1Machine::write16(std::uint32_t a, std::uint16_t d, std::uint16_t mask) {
    a &= ~1u;
    if (cps2_) {
        write16Cps2(a, d, mask);
        return;
    }
    auto combine = [&](std::uint16_t& dst) { dst = static_cast<std::uint16_t>((dst & ~mask) | (d & mask)); };
    if (a >= 0xff0000) { combine(mainRam_[(a & 0xffff) >> 1]); return; }
    if (a >= 0x900000 && a < 0x930000) { combine(gfxram_[(a - 0x900000) >> 1]); return; }
    if (a >= 0x800100 && a < 0x800140) { cpsAWrite(static_cast<int>((a - 0x800100) >> 1), d, mask); return; }
    if (a >= 0x800140 && a < 0x800180) { cpsBWrite(static_cast<int>((a - 0x800140) >> 1), d, mask); return; }
    if (a >= 0x800180 && a < 0x800188) {
        if (mask & 0x00ff) soundLatch_ = static_cast<std::uint8_t>(d & 0xff);  // low byte only, as on the board
        return;
    }
    if (a >= 0x800188 && a < 0x800190) {
        if (mask & 0x00ff) soundLatch2_ = static_cast<std::uint8_t>(d & 0xff);
        return;
    }
    // 0x800030 coin counters/lockout and anything else: no effect needed.
}

std::uint16_t Cps1Machine::cpsBRead(int off) {
    const auto& b = def_.board;
    if (cps2_) {  // CPS-B-21 raster counters
        if (off == 0x0e / 2) return static_cast<std::uint16_t>(((rasterCounter_[2] << 1) & 0x1fe) | (cpsB_[0x0e / 2] & 1));
        if (off == 0x10 / 2 || off == 0x12 / 2) return static_cast<std::uint16_t>(rasterCounter_[off & 1] & 0x1ff);
    }
    if (b.cpsbAddr >= 0 && off == b.cpsbAddr / 2) return static_cast<std::uint16_t>(b.cpsbValue);
    if (b.multLo >= 0 && off == b.multLo / 2)
        return static_cast<std::uint16_t>((cpsB_[b.multFactor1 / 2] * cpsB_[b.multFactor2 / 2]) & 0xffff);
    if (b.multHi >= 0 && off == b.multHi / 2)
        return static_cast<std::uint16_t>((static_cast<std::uint32_t>(cpsB_[b.multFactor1 / 2]) *
                                           cpsB_[b.multFactor2 / 2]) >> 16);
    if (b.in2Addr >= 0 && off == b.in2Addr / 2) return static_cast<std::uint16_t>(0xff00 | in2_);
    return 0xffff;
}

void Cps1Machine::cpsBWrite(int off, std::uint16_t d, std::uint16_t mask) {
    if (off < 0 || off >= 0x20) return;
    cpsB_[static_cast<size_t>(off)] = static_cast<std::uint16_t>((cpsB_[static_cast<size_t>(off)] & ~mask) | (d & mask));
    if (cps2_) {
        const std::uint16_t v = cpsB_[static_cast<size_t>(off)];
        if (off == 0x0e / 2) rasterReload_[2] = (v >> 1) & 0xff;
        if (off == 0x10 / 2 || off == 0x12 / 2) {
            rasterReload_[off & 1] = v & 0x1ff;
            if (v & 0x8000) rasterCounter_[off & 1] = rasterReload_[off & 1];  // manual reload
        }
    }
}

void Cps1Machine::cpsAWrite(int off, std::uint16_t d, std::uint16_t mask) {
    if (off < 0 || off >= 0x20) return;
    cpsA_[static_cast<size_t>(off)] = static_cast<std::uint16_t>((cpsA_[static_cast<size_t>(off)] & ~mask) | (d & mask));
    if (off == 0x05) buildPalette();  // writing the palette base copies the palette
}

// ---- sound CPU memory map ----

std::uint8_t Cps1Machine::z80Read(std::uint16_t a) {
    if (a < 0x8000) return (*audioRom_)[a];
    if (a < 0xc000) return (*audioRom_)[0x10000u + static_cast<unsigned>(audioBank_) * 0x4000u + (a - 0x8000u)];
    if (a >= 0xd000 && a < 0xd800) return z80Ram_[a - 0xd000u];
    switch (a) {
        case 0xf000:
        case 0xf001: return ym_->chip.read(a & 1);
        case 0xf002: return oki_.read();
        case 0xf008: return soundLatch_;
        case 0xf00a: return soundLatch2_;
        default: return 0xff;
    }
}

void Cps1Machine::z80Write(std::uint16_t a, std::uint8_t v) {
    if (a >= 0xd000 && a < 0xd800) { z80Ram_[a - 0xd000u] = v; return; }
    switch (a) {
        case 0xf000:
        case 0xf001: ym_->chip.write(a & 1, v); break;
        case 0xf002: oki_.write(v); break;
        case 0xf004: audioBank_ = v & 1; break;
        case 0xf006: oki_.setPin7(v & 1); break;
        default: break;
    }
}

void Cps1Machine::runSoundUntil(double target) {
    ::z80* z = z80_.get();
    while (static_cast<double>(z->cyc) < target) {
        if (ym_->irq && !z->int_pending) z80_gen_int(z, 0xff);
        z80_step(z);
    }
}

void Cps1Machine::generateAudio(int samples) {
    const double ymRate = kYmClock / 64.0;
    const double okiStep = oki_.sampleRate(kOkiClock) / ymRate;
    for (int i = 0; i < samples; ++i) {
        ym_->advance(64);
        ymfm::ym2151::output_data out;
        ym_->chip.generate(&out, 1);
        okiPhase_ += okiStep;
        while (okiPhase_ >= 1.0) {
            okiSample_ = oki_.next();
            okiPhase_ -= 1.0;
        }
        // Mono cabinet speaker: both FM outputs and the ADPCM channel mixed.
        const double mono = (out.data[0] + out.data[1]) * 0.35 + okiSample_ * 0.30;
        const auto s = static_cast<std::int16_t>(std::clamp(mono, -32768.0, 32767.0));
        audio_.push_back(s);
        audio_.push_back(s);
    }
}

void Cps1Machine::takeAudio(std::vector<std::int16_t>& out) {
    out.insert(out.end(), audio_.begin(), audio_.end());
    audio_.clear();
}

// ---- frame loop ----

void Cps1Machine::runFrame() {
    if (cps2_) {
        runFrameCps2();
        return;
    }
    const double cpuPerLine = def_.cpuClock * kLineSeconds;
    const double z80PerLine = kZ80Clock * kLineSeconds;
    const double ymPerLine = kYmClock / 64.0 * kLineSeconds;
    for (int line = 0; line < kVTotal; ++line) {
        const auto target = cpu_->getClock() + static_cast<moira::i64>(std::lround(cpuPerLine));
        cpu_->executeUntil(target);
        z80Cycle_ += z80PerLine;
        runSoundUntil(z80Cycle_);
        ymSampleAcc_ += ymPerLine;
        const int n = static_cast<int>(ymSampleAcc_);
        ymSampleAcc_ -= n;
        generateAudio(n);

        if (line == kVBlankStart - 1) {
            renderFrame();        // what the monitor shows this frame
            latchObjects();       // sprite list for the next frame
            irqPending_ = true;   // vblank interrupt (level 2)
            cpu_->setIPL(2);
        }
    }
    // Keep the Z80 cycle counter small to avoid precision loss.
    if (z80_->cyc > 1'000'000'000UL) {
        z80_->cyc -= 900'000'000UL;
        z80Cycle_ -= 900'000'000.0;
    }
}

}  // namespace zen1::cps1
