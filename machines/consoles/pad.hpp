// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cabinet panel -> console pad. Panel buttons, in order: LP MP HP (top row)
// LK MK HK (bottom row). The bottom row carries a pad's main buttons, the
// top row repeats them (so either row works) unless the pad has six.
// The player button is Start; the coin pulse is not used by consoles.
#pragma once

#include "zen1/emu/machine.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace zen1::consoles {

enum Panel { LP = 0, MP = 1, HP = 2, LK = 3, MK = 4, HK = 5 };

inline bool held(const emu::PlayerInput& p, Panel b) { return p.button[static_cast<size_t>(b)]; }
// Two-button pads (NES B/A, Game Boy B/A, SMS 1/2, PC Engine II/I):
// first = LK or LP, second = MK or MP, third (Select) = HK or HP.
inline bool first(const emu::PlayerInput& p) { return held(p, LK) || held(p, LP); }
inline bool second(const emu::PlayerInput& p) { return held(p, MK) || held(p, MP); }
inline bool third(const emu::PlayerInput& p) { return held(p, HK) || held(p, HP); }

// RGB565 -> XRGB8888
inline std::uint32_t rgb565(std::uint16_t c) {
    const std::uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}

// Copies a w x h picture into the middle of the machine's fixed canvas
// (cw x ch), black around it: the display planner finds the picture area.
inline void blit(std::vector<std::uint32_t>& canvas, int cw, int ch, const std::uint32_t* src, int w, int h, int stride) {
    std::fill(canvas.begin(), canvas.end(), 0u);
    const int ox = (cw - w) / 2, oy = (ch - h) / 2;
    for (int y = 0; y < h && oy + y < ch; ++y) {
        if (oy + y < 0) continue;
        for (int x = 0; x < w && ox + x < cw; ++x)
            if (ox + x >= 0) canvas[static_cast<size_t>((oy + y) * cw + ox + x)] = src[static_cast<size_t>(y * stride + x)] & 0xffffffu;
    }
}

// Mono or stereo samples at `rate` -> the engine's stereo stream.
struct AudioOut {
    std::vector<std::int16_t> buf;
    void stereo(const std::int16_t* s, int frames) { buf.insert(buf.end(), s, s + frames * 2); }
    void take(std::vector<std::int16_t>& out) {
        out.insert(out.end(), buf.begin(), buf.end());
        buf.clear();
    }
};

}  // namespace zen1::consoles
