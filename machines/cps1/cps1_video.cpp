// SPDX-License-Identifier: GPL-3.0-or-later
//
// CPS1 video: palette, three scroll layers, sprites and layer priority.
// Rendering rules ported from MAME's cps1_v.cpp (BSD-3-Clause,
// copyright-holders: Paul Leaman), rewritten as a direct per-pixel renderer.
#include "cps1.hpp"

#include <algorithm>

namespace zen1::cps1 {

namespace {
constexpr int kVideoControl = 0x22 / 2;
constexpr int kRowScrollOffs = 0x20 / 2;
constexpr std::uint16_t kBackgroundPen = 0x0bff;
}  // namespace

void Cps1Machine::decodeGfx() {
    // Each 4-byte group holds 8 pixels, one bit-plane per byte; byte 3 is the
    // most significant plane (MAME layout planes {24,16,8,0}, MSB-first bits).
    const auto& rom = *gfxRom_;
    gfxPixels_.assign(rom.size() * 2, 0);
    for (size_t g = 0; g + 3 < rom.size(); g += 4) {
        const std::uint8_t b0 = rom[g], b1 = rom[g + 1], b2 = rom[g + 2], b3 = rom[g + 3];
        std::uint8_t* out = &gfxPixels_[g * 2];
        for (int x = 0; x < 8; ++x) {
            const int s = 7 - x;
            out[x] = static_cast<std::uint8_t>((((b3 >> s) & 1) << 3) | (((b2 >> s) & 1) << 2) |
                                               (((b1 >> s) & 1) << 1) | ((b0 >> s) & 1));
        }
    }
}

const std::uint16_t* Cps1Machine::base(int reg, int boundary) const {
    int b = cpsA_[static_cast<size_t>(reg)] * 256;
    b &= ~(boundary - 1);
    return &gfxram_[static_cast<size_t>((b & 0x3ffff) / 2)];
}

int Cps1Machine::bankMapper(int type, int code) const {
    int shift = 0;
    switch (type) {
        case GFX_SPRITES: shift = 1; break;
        case GFX_SCROLL1: shift = 0; break;
        case GFX_SCROLL2: shift = 1; break;
        case GFX_SCROLL3: shift = 3; break;
    }
    code <<= shift;
    const auto& b = def_.board;
    for (const auto& r : b.mapper) {
        if (code >= r.start && code <= r.end && (r.type & type)) {
            int baseCode = 0;
            for (int i = 0; i < r.bank; ++i) baseCode += b.bankSizes[static_cast<size_t>(i)];
            return (baseCode + (code & (b.bankSizes[static_cast<size_t>(r.bank)] - 1))) >> shift;
        }
    }
    return -1;
}

void Cps1Machine::buildPalette() {
    const std::uint16_t* ram = base(0x05, 0x400);
    const std::uint16_t* p = ram;
    const std::uint16_t ctrl = cpsB_[static_cast<size_t>(def_.board.paletteControl / 2)];
    for (int page = 0; page < 6; ++page) {
        if (ctrl & (1 << page)) {
            for (int i = 0; i < 0x200; ++i) {
                const std::uint16_t c = *p++;
                const int bright = 0x0f + ((c >> 12) << 1);
                const int r = ((c >> 8) & 0x0f) * 0x11 * bright / 0x2d;
                const int g = ((c >> 4) & 0x0f) * 0x11 * bright / 0x2d;
                const int bl = (c & 0x0f) * 0x11 * bright / 0x2d;
                palette_[static_cast<size_t>(page * 0x200 + i)] =
                    (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) |
                    static_cast<std::uint32_t>(bl);
            }
        } else if (p != ram) {
            p += 0x200;  // disabled pages after the first enabled one still take space
        }
    }
}

void Cps1Machine::latchObjects() {
    const std::uint16_t* obj = base(0x00, 0x800);
    std::copy(obj, obj + 0x400, bufferedObj_.begin());
}

// Draws one scroll layer (1..3). highOnly: instead of drawing, mark the
// pixels that sit in front of sprites (priority 1), as the hardware does
// for the layer under the sprite layer.
void Cps1Machine::drawTilemap(int layer, bool highOnly) {
    const auto& b = def_.board;
    const std::uint16_t layerctl = cpsB_[static_cast<size_t>(b.layerControl / 2)];
    const std::uint16_t vc = cpsA_[kVideoControl];
    const bool enabled = layer == 1 ? (layerctl & b.layerEnable[0]) != 0
                       : layer == 2 ? (layerctl & b.layerEnable[1]) && (vc & 0x04)
                                    : (layerctl & b.layerEnable[2]) && (vc & 0x08);
    if (!enabled) return;

    std::array<std::uint16_t, 4> highMask{};
    for (int g = 0; g < 4; ++g)
        highMask[static_cast<size_t>(g)] =
            b.priority[static_cast<size_t>(g)] >= 0 ? cpsB_[static_cast<size_t>(b.priority[static_cast<size_t>(g)] / 2)] : 0;

    const std::uint16_t* map = base(layer, 0x4000);
    const std::uint16_t* other = base(0x04, 0x800);
    const int size = layer == 1 ? 8 : layer == 2 ? 16 : 32;
    const int mapMask = size * 64 - 1;
    const int sx0 = cpsA_[static_cast<size_t>(4 + 2 * layer)];   // scroll X: 0x0c/0x10/0x14
    const int sy0 = cpsA_[static_cast<size_t>(5 + 2 * layer)];   // scroll Y: 0x0e/0x12/0x16
    const bool rowScroll = layer == 2 && (vc & 0x01);
    const int rowOffs = cpsA_[kRowScrollOffs];
    const int gfxType = layer == 1 ? GFX_SCROLL1 : layer == 2 ? GFX_SCROLL2 : GFX_SCROLL3;
    const int tileBytes = size == 8 ? 64 : size == 16 ? 128 : 512;
    const size_t groups = gfxPixels_.size() / 8;
    const int colourBase = layer == 1 ? 0x20 : layer == 2 ? 0x40 : 0x60;

    for (int vy = 0; vy < kHeight; ++vy) {
        const int by = vy + kYOff;
        int sx = sx0;
        if (rowScroll) sx += other[(by + rowOffs) & 0x3ff];
        const int py = (by + sy0) & mapMask;
        const int row = py / size;
        for (int vx = 0; vx < kWidth; ++vx) {
            const int px = (vx + kXOff + sx) & mapMask;
            const int col = px / size;
            int idx;
            if (layer == 1) idx = (row & 0x1f) + ((col & 0x3f) << 5) + ((row & 0x20) << 6);
            else if (layer == 2) idx = (row & 0x0f) + ((col & 0x3f) << 4) + ((row & 0x30) << 6);
            else idx = (row & 0x07) + ((col & 0x3f) << 3) + ((row & 0x38) << 6);

            int code = map[2 * idx];
            const std::uint16_t attr = map[2 * idx + 1];
            if (layer == 3) code &= 0x3fff;
            code = bankMapper(gfxType, code);
            if (code < 0) continue;  // empty tile: fully transparent

            int tx = px & (size - 1), ty = py & (size - 1);
            if (attr & 0x20) tx = size - 1 - tx;
            if (attr & 0x40) ty = size - 1 - ty;

            size_t byteOff;
            if (size == 8) byteOff = static_cast<size_t>(code) * 64 + static_cast<size_t>(ty) * 8 + ((idx >> 5) & 1) * 4;
            else if (size == 16) byteOff = static_cast<size_t>(code) * 128 + static_cast<size_t>(ty) * 8 + (tx >= 8 ? 4 : 0);
            else byteOff = static_cast<size_t>(code) * 512 + static_cast<size_t>(ty) * 16 + static_cast<size_t>(tx >> 3) * 4;
            (void)tileBytes;
            const size_t group = (byteOff / 4) % groups;
            const int pen = gfxPixels_[group * 8 + static_cast<size_t>(tx & 7)];
            const size_t o = static_cast<size_t>(vy * kWidth + vx);
            if (highOnly) {
                const int grp = (attr >> 7) & 3;
                if (highMask[static_cast<size_t>(grp)] & (1 << pen)) prio_[o] |= 1;
            } else if (pen != 15) {
                pens_[o] = static_cast<std::uint16_t>(((attr & 0x1f) + colourBase) * 16 + pen);
            }
        }
    }
}

void Cps1Machine::drawSprites() {
    // Last sprite: attribute 0xffxx terminates the list.
    int last = 0x400 - 4;
    for (int off = 0; off < 0x400; off += 4) {
        if ((bufferedObj_[static_cast<size_t>(off + 3)] & 0xff00) == 0xff00) {
            last = off - 4;
            break;
        }
    }
    const size_t elements = gfxPixels_.size() / 256;  // 16x16 tiles of 256 pixels
    if (elements == 0) return;

    auto draw = [&](int code, int colour, bool flipx, bool flipy, int sx, int sy) {
        const size_t base = (static_cast<size_t>(code) % elements) * 128;  // bytes per tile
        for (int ty = 0; ty < 16; ++ty) {
            const int by = sy + ty - kYOff;
            if (by < 0 || by >= kHeight) continue;
            const int srcY = flipy ? 15 - ty : ty;
            for (int tx = 0; tx < 16; ++tx) {
                const int bx = sx + tx - kXOff;
                if (bx < 0 || bx >= kWidth) continue;
                const int srcX = flipx ? 15 - tx : tx;
                const size_t byteOff = base + static_cast<size_t>(srcY) * 8 + (srcX >= 8 ? 4 : 0);
                const int pen = gfxPixels_[(byteOff / 4) * 8 + static_cast<size_t>(srcX & 7)];
                if (pen == 15) continue;
                const size_t o = static_cast<size_t>(by * kWidth + bx);
                // Hidden behind high-priority tile pixels (priority 1); every
                // opaque sprite pixel marks the spot (priority 31).
                if (((1u << (prio_[o] & 31)) & 0x02u) == 0)
                    pens_[o] = static_cast<std::uint16_t>(colour * 16 + pen);
                prio_[o] = 31;
            }
        }
    };

    for (int i = last; i >= 0; i -= 4) {
        const int x = bufferedObj_[static_cast<size_t>(i)];
        const int y = bufferedObj_[static_cast<size_t>(i + 1)];
        int code = bufferedObj_[static_cast<size_t>(i + 2)];
        const int colour = bufferedObj_[static_cast<size_t>(i + 3)];
        const int col = colour & 0x1f;
        const bool flipx = colour & 0x20, flipy = colour & 0x40;
        code = bankMapper(GFX_SPRITES, code);
        if (code == -1) continue;
        if (colour & 0xff00) {  // block of nx x ny tiles
            const int nx = ((colour & 0x0f00) >> 8) + 1;
            const int ny = ((colour & 0xf000) >> 12) + 1;
            for (int nys = 0; nys < ny; ++nys) {
                const int sy = (y + nys * 16) & 0x1ff;
                for (int nxs = 0; nxs < nx; ++nxs) {
                    const int sx = (x + nxs * 16) & 0x1ff;
                    const int cx = flipx ? ((code + (nx - 1) - nxs) & 0xf) : ((code + nxs) & 0xf);
                    const int cy = flipy ? 0x10 * (ny - 1 - nys) : 0x10 * nys;
                    draw((code & ~0xf) + cx + cy, col, flipx, flipy, sx, sy);
                }
            }
        } else {
            draw(code, col, flipx, flipy, x & 0x1ff, y & 0x1ff);
        }
    }
}

void Cps1Machine::renderFrame() {
    std::fill(pens_.begin(), pens_.end(), kBackgroundPen);
    std::fill(prio_.begin(), prio_.end(), 0);

    const std::uint16_t layerctl = cpsB_[static_cast<size_t>(def_.board.layerControl / 2)];
    const int order[4] = {(layerctl >> 6) & 3, (layerctl >> 8) & 3, (layerctl >> 10) & 3, (layerctl >> 12) & 3};
    auto drawLayer = [&](int l) {
        if (l == 0) drawSprites();
        else drawTilemap(l, false);
    };
    drawLayer(order[0]);
    for (int i = 1; i < 4; ++i) {
        if (order[i] == 0 && order[i - 1] != 0) drawTilemap(order[i - 1], true);  // mask for sprites
        drawLayer(order[i]);
    }
    for (size_t i = 0; i < pens_.size(); ++i) frame_[i] = palette_[pens_[i] % palette_.size()];
}

}  // namespace zen1::cps1
