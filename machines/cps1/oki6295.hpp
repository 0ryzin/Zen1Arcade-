// SPDX-License-Identifier: GPL-3.0-or-later
//
// OKI MSM6295 4-voice ADPCM sample player (Zen1 implementation, written from
// the chip's documented behaviour: phrase table, command protocol and
// Dialogic/OKI 4-bit ADPCM).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace zen1::cps1 {

class Oki6295 {
public:
    void setRom(const std::vector<std::uint8_t>* rom) { rom_ = rom; }
    void reset();
    void write(std::uint8_t data);
    std::uint8_t read() const;
    void setPin7(bool high) { pin7High_ = high; }

    // Output rate for a given master clock (1 MHz on CPS1).
    double sampleRate(double clock) const { return clock / (pin7High_ ? 132.0 : 165.0); }

    // Produces the next output sample (all voices mixed, 16-bit range).
    std::int32_t next();

private:
    struct Adpcm {
        std::int32_t signal = -2;
        std::int32_t step = 0;
        void reset() { signal = -2; step = 0; }
        std::int32_t clock(std::uint8_t nibble);
    };
    struct Voice {
        bool playing = false;
        std::uint32_t base = 0;
        std::uint32_t sample = 0;
        std::uint32_t count = 0;
        std::int32_t volume = 0;
        Adpcm adpcm;
    };
    std::uint8_t romByte(std::uint32_t addr) const;

    const std::vector<std::uint8_t>* rom_ = nullptr;
    std::array<Voice, 4> voices_{};
    int command_ = -1;
    bool pin7High_ = true;
};

}  // namespace zen1::cps1
