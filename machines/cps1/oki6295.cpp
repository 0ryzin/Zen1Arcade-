// SPDX-License-Identifier: GPL-3.0-or-later
#include "oki6295.hpp"

#include <algorithm>
#include <cmath>

namespace zen1::cps1 {
namespace {

struct Tables {
    std::array<std::int32_t, 49 * 16> diff{};
    Tables() {
        for (int step = 0; step <= 48; ++step) {
            const int stepval = static_cast<int>(std::floor(16.0 * std::pow(11.0 / 10.0, step)));
            for (int nib = 0; nib < 16; ++nib) {
                const int sign = (nib & 8) ? -1 : 1;
                const int v = stepval * ((nib >> 2) & 1) + stepval / 2 * ((nib >> 1) & 1) +
                              stepval / 4 * (nib & 1) + stepval / 8;
                diff[static_cast<size_t>(step * 16 + nib)] = sign * v;
            }
        }
    }
};
const Tables& tables() {
    static const Tables t;
    return t;
}
constexpr std::int32_t kIndexShift[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
// Attenuation 0..8 in 3 dB steps, out of 0x20; 9..15 silent.
constexpr std::int32_t kVolume[16] = {0x20, 0x16, 0x10, 0x0b, 0x08, 0x06, 0x04, 0x03,
                                      0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

}  // namespace

std::int32_t Oki6295::Adpcm::clock(std::uint8_t nibble) {
    signal += tables().diff[static_cast<size_t>(step * 16 + (nibble & 15))];
    signal = std::clamp(signal, -2048, 2047);
    step = std::clamp(step + kIndexShift[nibble & 7], 0, 48);
    return signal;
}

void Oki6295::reset() {
    for (auto& v : voices_) v = Voice{};
    command_ = -1;
}

std::uint8_t Oki6295::romByte(std::uint32_t addr) const {
    if (!rom_ || rom_->empty()) return 0;
    return (*rom_)[addr % rom_->size()];
}

void Oki6295::write(std::uint8_t data) {
    if (command_ != -1) {
        // Second byte: which voices start this phrase, and their attenuation.
        const int mask = data >> 4;
        for (int i = 0; i < 4; ++i) {
            if (!(mask & (1 << i))) continue;
            auto& v = voices_[static_cast<size_t>(i)];
            const std::uint32_t t = static_cast<std::uint32_t>(command_) * 8;
            const std::uint32_t start = ((romByte(t) << 16) | (romByte(t + 1) << 8) | romByte(t + 2)) & 0x3ffff;
            const std::uint32_t stop = ((romByte(t + 3) << 16) | (romByte(t + 4) << 8) | romByte(t + 5)) & 0x3ffff;
            if (!v.playing && start < stop) {
                v.playing = true;
                v.base = start;
                v.sample = 0;
                v.count = 2 * (stop - start + 1);
                v.adpcm.reset();
                v.volume = kVolume[data & 0x0f];
            }
        }
        command_ = -1;
    } else if (data & 0x80) {
        command_ = data & 0x7f;  // phrase number; voice mask follows
    } else {
        for (int i = 0; i < 4; ++i)
            if (data & (0x08 << i)) voices_[static_cast<size_t>(i)].playing = false;
    }
}

std::uint8_t Oki6295::read() const {
    std::uint8_t r = 0xf0;
    for (int i = 0; i < 4; ++i)
        if (voices_[static_cast<size_t>(i)].playing) r |= static_cast<std::uint8_t>(1 << i);
    return r;
}

std::int32_t Oki6295::next() {
    std::int32_t out = 0;
    for (auto& v : voices_) {
        if (!v.playing) continue;
        const std::uint8_t byte = romByte(v.base + v.sample / 2);
        const std::uint8_t nibble = (v.sample & 1) ? (byte & 0x0f) : (byte >> 4);
        out += v.adpcm.clock(nibble) * v.volume / 2;
        if (++v.sample >= v.count) v.playing = false;
    }
    return out;
}

}  // namespace zen1::cps1
