// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom CPS-2 for the Zen1 engine: memory map, QSound sound CPU, raster
// interrupts, serial EEPROM and program decryption glue. The tile layers and
// palette are the CPS-1 ones (cps1_video.cpp); the CPS-2 sprite chip is in
// cps1_video.cpp too. Hardware behaviour ported from MAME's cps2.cpp and
// cps1.cpp (BSD-3-Clause, copyright-holders: Paul Leaman, Andreas Naive,
// Nicola Salmoria). See THIRD_PARTY.md.
#include "cps1.hpp"
#include "cps1_cpu.hpp"
#include "cps2crypt.hpp"

#include <cstddef>
#include <Moira.h>

extern "C" {
#include <z80.h>
}

#include <algorithm>
#include <cmath>

namespace zen1::cps1 {

namespace {
constexpr double kPixelClock = 8'000'000.0;
constexpr int kHTotal = 512, kVTotal = 262, kVBlankLine = 240, kFirstVisible = 16;
constexpr double kLineSeconds = kHTotal / kPixelClock;
constexpr double kCpuClock = 16'000'000.0 * 0.7375;  // MAME's clock scale for unemulated wait states
constexpr double kZ80Clock = 8'000'000.0;
constexpr double kZ80IrqHz = 250.0;
// Object output registers (word index from 0x400000).
constexpr int kObjPri = 2, kObjXoffs = 4, kObjYoffs = 5;
}  // namespace

// ------------------------------------------------------------- EEPROM ---

void Eeprom93c46::write(bool cs, bool clk, bool di) {
    if (!cs) {
        cs_ = false;
        bits_ = 0;
        shift_ = 0;
        readWord_ = -1;
        out_ = true;  // ready
        clk_ = clk;
        return;
    }
    cs_ = true;
    if (clk && !clk_) {  // rising edge
        if (readWord_ >= 0) {
            // Shifting a word out, most significant bit first.
            out_ = (data[static_cast<size_t>(readWord_)] >> (15 - readBit_)) & 1;
            if (++readBit_ == 16) {  // sequential read continues with the next word
                readBit_ = 0;
                readWord_ = (readWord_ + 1) & 63;
            }
        } else if (bits_ > 0 || di) {
            // start bit (1) + 2 opcode bits + 6 address bits [+ 16 data bits];
            // zeros before the start bit are ignored.
            shift_ = (shift_ << 1) | (di ? 1u : 0u);
            ++bits_;
            command();
        }
    }
    clk_ = clk;
}

void Eeprom93c46::command() {
    if (bits_ < 9) return;
    const int op = static_cast<int>((shift_ >> (bits_ - 3)) & 3);
    const int addr = static_cast<int>((shift_ >> (bits_ - 9)) & 63);
    if (op == 2 && bits_ == 9) {  // READ: a dummy 0, then the data
        readWord_ = addr;
        readBit_ = 0;
        out_ = false;
        return;
    }
    if (op == 0 && bits_ == 9) {
        const int sub = addr >> 4;
        if (sub == 3) writeEnable_ = true;   // EWEN
        if (sub == 0) writeEnable_ = false;  // EWDS
        if (sub == 2 && writeEnable_) data.fill(0xffff);  // ERAL
        if (sub != 1) { bits_ = 0; shift_ = 0; }
        return;
    }
    if (op == 3 && bits_ == 9) {  // ERASE
        if (writeEnable_) data[static_cast<size_t>(addr)] = 0xffff;
        bits_ = 0;
        shift_ = 0;
        return;
    }
    if (bits_ == 25) {
        const auto value = static_cast<std::uint16_t>(shift_ & 0xffff);
        const int op2 = static_cast<int>((shift_ >> 22) & 3);
        const int addr2 = static_cast<int>((shift_ >> 16) & 63);
        if (writeEnable_) {
            if (op2 == 1) data[static_cast<size_t>(addr2)] = value;  // WRITE
            if (op2 == 0 && (addr2 >> 4) == 1) data.fill(value);   // WRAL
        }
        out_ = true;  // ready straight away
        bits_ = 0;
        shift_ = 0;
    }
}

std::vector<std::uint8_t> Cps1Machine::nvram() const {
    if (!cps2_) return {};
    std::vector<std::uint8_t> v;
    for (auto w : eeprom_.data) {
        v.push_back(static_cast<std::uint8_t>(w >> 8));
        v.push_back(static_cast<std::uint8_t>(w));
    }
    return v;
}

void Cps1Machine::setNvram(const std::vector<std::uint8_t>& d) {
    if (!cps2_ || d.size() != 128) return;
    for (size_t i = 0; i < 64; ++i) eeprom_.data[i] = static_cast<std::uint16_t>((d[2 * i] << 8) | d[2 * i + 1]);
}

// ---------------------------------------------------------- decryption ---

void Cps1Machine::decryptProgram() {
    const auto& rom = *mainRom_;
    const auto& keyFile = regions_["key"];
    std::uint32_t key[2], lower = 0, upper = 0;
    std::vector<std::uint16_t> words(rom.size() / 2);
    for (size_t i = 0; i < words.size(); ++i) words[i] = static_cast<std::uint16_t>((rom[2 * i] << 8) | rom[2 * i + 1]);
    decrypted_ = words;
    if (!cps2crypt::keyFromFile(keyFile, key, lower, upper)) return;  // no key: plain program (homebrew)
    cps2crypt::decrypt(words.data(), decrypted_.data(), static_cast<int>(rom.size()), key, lower / 2, upper / 2);
}

bool Cps1Machine::programFetch(std::uint32_t) const { return !decrypted_.empty(); }

// ---------------------------------------------------------- 68000 map ---

std::uint16_t Cps1Machine::read16Cps2(std::uint32_t a) {
    if (a < 0x400000) return static_cast<std::uint16_t>(((*mainRom_)[a] << 8) | (*mainRom_)[a + 1]);
    if (a >= 0xff0000) {
        if (def_.cps2Dead && a >= 0xfffff0 && a < 0xfffffc) return output_[(a - 0xfffff0) >> 1];
        return mainRam_[(a & 0xffff) >> 1];
    }
    if (a >= 0x900000 && a < 0x930000) return gfxram_[(a - 0x900000) >> 1];
    if (!def_.cps2Dead && a >= 0x400000 && a < 0x40000c) return output_[(a - 0x400000) >> 1];
    if (a >= 0x618000 && a < 0x61a000) return static_cast<std::uint16_t>(0xff00 | qram_[0][(a - 0x618000) >> 1]);
    if (a >= 0x660000 && a < 0x664002) return extraRam_[(a - 0x660000) >> 1];
    if (a >= 0x708000 && a < 0x710000) return objram_[static_cast<size_t>(objBank_ ^ 1)][((a - 0x708000) >> 1) & 0xfff];
    if ((a >= 0x800140 && a < 0x800180)) return cpsBRead(static_cast<int>((a - 0x800140) >> 1));
    if ((a >= 0x804140 && a < 0x804180)) return cpsBRead(static_cast<int>((a - 0x804140) >> 1));
    switch (a) {
        case 0x804000: return cps2In0_;
        case 0x804010: return cps2In1_;
        case 0x804020: return static_cast<std::uint16_t>((cps2In2_ & ~1u) | (eeprom_.dataOut() ? 1 : 0));
        case 0x804030: return 0xe021;  // full digital volume; no network board, no extra RAM board
        default: break;
    }
    return 0xffff;
}

void Cps1Machine::write16Cps2(std::uint32_t a, std::uint16_t d, std::uint16_t mask) {
    auto combine = [&](std::uint16_t& dst) { dst = static_cast<std::uint16_t>((dst & ~mask) | (d & mask)); };
    if (a >= 0xff0000) {
        if (def_.cps2Dead && a >= 0xfffff0 && a < 0xfffffc) { combine(output_[(a - 0xfffff0) >> 1]); return; }
        combine(mainRam_[(a & 0xffff) >> 1]);
        return;
    }
    if (a >= 0x900000 && a < 0x930000) { combine(gfxram_[(a - 0x900000) >> 1]); return; }
    if (!def_.cps2Dead && a >= 0x400000 && a < 0x40000c) { combine(output_[(a - 0x400000) >> 1]); return; }
    if (a >= 0x618000 && a < 0x61a000) {
        if (mask & 0x00ff) qram_[0][(a - 0x618000) >> 1] = static_cast<std::uint8_t>(d);
        return;
    }
    if (a >= 0x660000 && a < 0x664002) { combine(extraRam_[(a - 0x660000) >> 1]); return; }
    if (a >= 0x700000 && a < 0x702000) { combine(objram_[static_cast<size_t>(objBank_)][(a - 0x700000) >> 1]); return; }
    if (a >= 0x708000 && a < 0x710000) {
        combine(objram_[static_cast<size_t>(objBank_ ^ 1)][((a - 0x708000) >> 1) & 0xfff]);
        return;
    }
    if (a >= 0x800100 && a < 0x800140) { cpsAWrite(static_cast<int>((a - 0x800100) >> 1), d, mask); return; }
    if (a >= 0x804100 && a < 0x804140) { cpsAWrite(static_cast<int>((a - 0x804100) >> 1), d, mask); return; }
    if (a >= 0x800140 && a < 0x800180) { cpsBWrite(static_cast<int>((a - 0x800140) >> 1), d, mask); return; }
    if (a >= 0x804140 && a < 0x804180) { cpsBWrite(static_cast<int>((a - 0x804140) >> 1), d, mask); return; }
    if (a == 0x804040) {
        if (mask & 0xff00) eeprom_.write(d & 0x4000, d & 0x2000, d & 0x1000);  // CS, CLK, DI
        if (mask & 0x00ff) z80Held_ = !(d & 0x08);                             // bit 3: Z80 runs
        return;
    }
    if (a == 0x8040e0) {
        if (mask & 0x00ff) objBank_ = d & 1;
        return;
    }
}

// ------------------------------------------------------ QSound Z80 map ---

std::uint8_t Cps1Machine::z80ReadCps2(std::uint16_t a) {
    if (a < 0x8000) return (*audioRom_)[a];
    if (a < 0xc000) {
        const size_t o = 0x10000u + static_cast<size_t>(audioBank_) * 0x4000u + (a - 0x8000u);
        return o < audioRom_->size() ? (*audioRom_)[o] : 0xff;
    }
    if (a < 0xd000) return qram_[0][a - 0xc000u];
    if (a == 0xd007) return qsound_.qsound_r();
    if (a >= 0xf000) return qram_[1][a - 0xf000u];
    return 0xff;
}

void Cps1Machine::z80WriteCps2(std::uint16_t a, std::uint8_t v) {
    if (a >= 0xc000 && a < 0xd000) { qram_[0][a - 0xc000u] = v; return; }
    if (a >= 0xf000) { qram_[1][a - 0xf000u] = v; return; }
    if (a >= 0xd000 && a <= 0xd002) { qsound_.qsound_w(a - 0xd000u, v); return; }
    if (a == 0xd003) {
        int bank = v & 0x0f;
        if (0x10000u + static_cast<unsigned>(bank) * 0x4000u >= audioRom_->size()) bank = 0;
        audioBank_ = bank;
    }
}

// ----------------------------------------------------------- frame loop ---

void Cps1Machine::latchCps2Objects() {
    priCtrl_ = output_[kObjPri];
    // The object base register picks the bank; games swap banks with 8040e0.
    std::copy(objram_[static_cast<size_t>(objBank_)].begin(), objram_[static_cast<size_t>(objBank_)].end(), bufferedObj2_.begin());
}

void Cps1Machine::runFrameCps2() {
    const double cpuPerLine = kCpuClock * kLineSeconds;
    const double z80PerLine = kZ80Clock * kLineSeconds;
    const double qsPerLine = QSound::kSampleRate * kLineSeconds;
    const double irqPerLine = kZ80IrqHz * kLineSeconds;
    renderedRows_ = 0;
    auto renderTo = [&](int row) {
        row = std::clamp(row, 0, kHeight);
        if (row > renderedRows_) {
            renderLines(renderedRows_, row);
            renderedRows_ = row;
        }
    };
    for (int line = 0; line < kVTotal; ++line) {
        // CPS-B-21 raster counters count lines down; reaching 0 raises IRQ 4.
        bool raster = false;
        for (int i = 0; i < 2; ++i) {
            rasterCounter_[i] = line == 0 ? rasterReload_[i] : (rasterCounter_[i] - 1) & 0x1ff;
            if (rasterCounter_[i] == 0) raster = true;
        }
        rasterCounter_[2] = rasterReload_[2];
        if (raster) {
            renderTo(line - kFirstVisible);  // the lines above use the old registers
            ipl_ |= 4;
        }
        if (line == kVBlankLine) {
            renderTo(kHeight);
            latchCps2Objects();
            ipl_ |= 2;
        }
        cpu_->setIPL(static_cast<moira::u8>(ipl_ == 6 ? 6 : ipl_ & 4 ? 4 : ipl_ & 2 ? 2 : 0));

        const auto target = cpu_->getClock() + static_cast<moira::i64>(std::lround(cpuPerLine));
        cpu_->executeUntil(target);

        // Sound CPU (250 Hz interrupt) and QSound samples.
        z80IrqAcc_ += irqPerLine;
        if (z80IrqAcc_ >= 1.0) {
            z80IrqAcc_ -= 1.0;
            if (!z80Held_) z80_gen_int(z80_.get(), 0xff);
        }
        z80Cycle_ += z80PerLine;
        if (z80Held_) {
            z80_->cyc = static_cast<unsigned long>(z80Cycle_);
        } else {
            ::z80* z = z80_.get();
            while (static_cast<double>(z->cyc) < z80Cycle_) z80_step(z);
        }
        qsAcc_ += qsPerLine;
        while (qsAcc_ >= 1.0) {
            qsAcc_ -= 1.0;
            std::int16_t l = 0, r = 0;
            qsound_.generate(l, r);
            audio_.push_back(l);
            audio_.push_back(r);
        }
    }
    if (z80_->cyc > 1'000'000'000UL) {
        z80_->cyc -= 900'000'000UL;
        z80Cycle_ -= 900'000'000.0;
    }
}

}  // namespace zen1::cps1
