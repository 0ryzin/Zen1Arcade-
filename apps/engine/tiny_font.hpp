// SPDX-License-Identifier: GPL-3.0-or-later
// 3x5 pixel font (A-Z, 0-9, a few symbols) for diagnostic overlays drawn
// into a native-resolution frame buffer. Original glyph set.
#pragma once

#include <cstdint>
#include <string_view>

namespace zen1::engine {

struct Frame {
    std::uint32_t* pixels = nullptr;  // XRGB8888
    int width = 0;
    int height = 0;
    void put(int x, int y, std::uint32_t c) {
        if (x >= 0 && y >= 0 && x < width && y < height) pixels[y * width + x] = c;
    }
    void fill(int x, int y, int w, int h, std::uint32_t c) {
        for (int j = y; j < y + h; ++j)
            for (int i = x; i < x + w; ++i) put(i, j, c);
    }
};

// Draws text with its top-left at (x, y); scale = pixel size of one font dot.
// Returns the x position after the last character.
int drawText(Frame& f, int x, int y, std::string_view text, std::uint32_t colour, int scale = 1);

}  // namespace zen1::engine
