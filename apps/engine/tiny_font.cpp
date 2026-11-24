// SPDX-License-Identifier: GPL-3.0-or-later
#include "tiny_font.hpp"

#include <cctype>

namespace zen1::engine {
namespace {

// Each glyph: 5 rows of 3 bits (bit 2 = left column).
struct Glyph {
    char c;
    std::uint8_t rows[5];
};

constexpr Glyph kGlyphs[] = {
    {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}}, {'C', {3, 4, 4, 4, 3}},
    {'D', {6, 5, 5, 5, 6}}, {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
    {'G', {3, 4, 5, 5, 3}}, {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}},
    {'J', {1, 1, 1, 5, 2}}, {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}},
    {'M', {5, 7, 7, 5, 5}}, {'N', {6, 5, 5, 5, 5}}, {'O', {2, 5, 5, 5, 2}},
    {'P', {6, 5, 6, 4, 4}}, {'Q', {2, 5, 5, 6, 3}}, {'R', {6, 5, 6, 5, 5}},
    {'S', {3, 4, 2, 1, 6}}, {'T', {7, 2, 2, 2, 2}}, {'U', {5, 5, 5, 5, 7}},
    {'V', {5, 5, 5, 5, 2}}, {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}},
    {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}}, {'0', {7, 5, 5, 5, 7}},
    {'1', {2, 6, 2, 2, 7}}, {'2', {6, 1, 2, 4, 7}}, {'3', {6, 1, 2, 1, 6}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 6, 1, 6}}, {'6', {3, 4, 7, 5, 7}},
    {'7', {7, 1, 2, 2, 2}}, {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 6}},
    {'.', {0, 0, 0, 0, 2}}, {':', {0, 2, 0, 2, 0}}, {'-', {0, 0, 7, 0, 0}},
    {'/', {1, 1, 2, 4, 4}}, {'%', {5, 1, 2, 4, 5}}, {'(', {1, 2, 2, 2, 1}},
    {')', {4, 2, 2, 2, 4}}, {'+', {0, 2, 7, 2, 0}}, {'=', {0, 7, 0, 7, 0}},
};

const Glyph* find(char c) {
    const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const auto& g : kGlyphs)
        if (g.c == u) return &g;
    return nullptr;
}

}  // namespace

int drawText(Frame& f, int x, int y, std::string_view text, std::uint32_t colour, int scale) {
    for (char c : text) {
        if (const Glyph* g = find(c)) {
            for (int row = 0; row < 5; ++row)
                for (int col = 0; col < 3; ++col)
                    if (g->rows[row] & (4 >> col))
                        f.fill(x + col * scale, y + row * scale, scale, scale, colour);
        }
        x += 4 * scale;
    }
    return x;
}

}  // namespace zen1::engine
