// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zen1 cabinet test card, neon edition. 640x480 square pixels shown at 4:3.
// Checks, top to bottom:
//   corner arrows    all four must be visible (nothing cropped)
//   circle           must look round (correct picture shape)
//   colour/grey bars brightness and colour range
//   gliding bar      must move smoothly (frame pacing)
//   P1 / P2 panels   stick and every button light up live, with the arcade
//                    layout (LP MP HP / LK MK HK + player button) and the raw
//                    button numbers used by the remap screen
#include "test_card.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zen1::engine {
namespace {

constexpr std::uint32_t rgb(int r, int g, int b) {
    return (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) |
           static_cast<std::uint32_t>(b);
}
constexpr std::uint32_t kWhite = rgb(242, 240, 255);
constexpr std::uint32_t kDim = rgb(140, 136, 180);
constexpr std::uint32_t kCyan = rgb(41, 230, 255);
constexpr std::uint32_t kPink = rgb(255, 60, 200);
constexpr std::uint32_t kAmber = rgb(255, 194, 60);
constexpr std::uint32_t kGreen = rgb(70, 255, 154);
constexpr std::uint32_t kOff = rgb(34, 30, 64);

// Adds colour c with strength a (0..1) to a pixel: light, like a neon tube.
void glow(Frame& f, int x, int y, std::uint32_t c, double a) {
    if (x < 0 || y < 0 || x >= f.width || y >= f.height || a <= 0) return;
    std::uint32_t& p = f.pixels[y * f.width + x];
    auto ch = [&](int s) {
        const int v = static_cast<int>((p >> s) & 255) + static_cast<int>(((c >> s) & 255) * std::min(1.0, a));
        return static_cast<std::uint32_t>(std::min(255, v)) << s;
    };
    p = ch(16) | ch(8) | ch(0);
}

// Neon rectangle outline with a soft halo.
void neonRect(Frame& f, int x, int y, int w, int h, std::uint32_t c) {
    for (int d = -4; d <= 4; ++d) {
        const double a = d == 0 ? 1.0 : 0.28 / std::abs(d);
        for (int i = x - d; i < x + w + d; ++i) { glow(f, i, y - d, c, a); glow(f, i, y + h - 1 + d, c, a); }
        for (int j = y - d + 1; j < y + h + d - 1; ++j) { glow(f, x - d, j, c, a); glow(f, x + w - 1 + d, j, c, a); }
    }
}

void disc(Frame& f, int cx, int cy, int r, std::uint32_t c, bool lit) {
    for (int y = -r - 5; y <= r + 5; ++y)
        for (int x = -r - 5; x <= r + 5; ++x) {
            const double d = std::sqrt(x * x + y * y);
            if (d <= r - 2) f.put(cx + x, cy + y, lit ? c : rgb(16, 14, 34));
            else if (d <= r) f.put(cx + x, cy + y, lit ? kWhite : c);
            else if (lit) glow(f, cx + x, cy + y, c, 0.5 * std::exp(-(d - r) / 2.0));
        }
}

void textC(Frame& f, int cx, int y, std::string_view s, std::uint32_t c, int scale) {
    drawText(f, cx - static_cast<int>(s.size()) * 4 * scale / 2, y, s, c, scale);
}

void drawPanel(Frame& f, int x0, int y0, const PanelState& p, std::uint32_t accent, const char* title) {
    const int w = 300, h = 150;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) f.put(x, y, rgb(10, 8, 26));
    neonRect(f, x0, y0, w, h, p.connected ? accent : kDim);
    drawText(f, x0 + 10, y0 + 9, title, p.connected ? accent : kDim, 3);
    drawText(f, x0 + 10, y0 + 29, p.connected ? p.label : "NO PANEL - " + p.label, p.connected ? kWhite : kAmber, 1);

    // 8-way stick: arrows around a ball.
    const int sx = x0 + 52, sy = y0 + 82;
    disc(f, sx, sy, 30, rgb(60, 56, 110), false);
    for (int j = -1; j <= 1; ++j)
        for (int i = -1; i <= 1; ++i) {
            if (!i && !j) continue;
            const bool on = p.connected && i == p.dirX && j == p.dirY;
            disc(f, sx + i * 21, sy + j * 21, 5, on ? kAmber : kOff, on);
        }
    disc(f, sx + p.dirX * 9, sy + p.dirY * 9, 9, accent, p.connected);

    // Arcade layout from the button map: LP MP HP / LK MK HK, player button.
    static const char* kNames[7] = {"LP", "MP", "HP", "LK", "MK", "HK", "P"};
    for (int k = 0; k < 7; ++k) {
        const int idx = p.map[static_cast<size_t>(k)];
        const bool on = p.connected && idx >= 0 && idx < 12 && p.buttons[static_cast<size_t>(idx)];
        const int bx = k < 6 ? x0 + 120 + (k % 3) * 40 + (k >= 3 ? 8 : 0) : x0 + 270;
        const int by = k < 6 ? y0 + 60 + (k / 3) * 36 : y0 + 60;
        const std::uint32_t col = k < 3 ? kCyan : k < 6 ? kPink : kAmber;
        disc(f, bx, by, 14, col, on);
        textC(f, bx, by - 2, kNames[k], on ? rgb(10, 8, 26) : kWhite, 1);
    }
    // Raw button numbers 1-12 (what the remap screen records).
    for (int b = 0; b < 12; ++b) {
        const bool on = p.connected && p.buttons[static_cast<size_t>(b)];
        const int bx = x0 + 14 + b * 23, by = y0 + 128;
        for (int y = 0; y < 14; ++y)
            for (int x = 0; x < 19; ++x) f.put(bx + x, by + y, on ? kAmber : kOff);
        char num[4];
        std::snprintf(num, sizeof num, "%d", b + 1);
        textC(f, bx + 10, by + 5, num, on ? rgb(10, 8, 26) : kDim, 1);
    }
}

}  // namespace

void drawTestCard(Frame& f, const CardInfo& info) {
    // Night-sky gradient with a neon horizon and grid floor.
    const int horizon = 250;
    for (int y = 0; y < f.height; ++y) {
        const double t = static_cast<double>(y) / f.height;
        const std::uint32_t c = rgb(static_cast<int>(6 + 22 * t), static_cast<int>(4 + 6 * t), static_cast<int>(18 + 34 * t));
        for (int x = 0; x < f.width; ++x) f.pixels[y * f.width + x] = c;
        const double g = std::exp(-std::pow((y - horizon) / 6.0, 2));
        for (int x = 0; x < f.width; ++x) glow(f, x, y, kPink, g * 0.8);
    }
    const double scroll = std::fmod(static_cast<double>(info.frame) / 60.0, 1.0);
    for (int i = 0; i < 12; ++i) {
        const double d = (i + scroll) / 12.0;
        const int y = horizon + static_cast<int>((f.height - horizon) * d * d);
        for (int x = 0; x < f.width; ++x) glow(f, x, y, kPink, 0.15 + 0.35 * d);
    }
    for (int i = -10; i <= 10; ++i)
        for (int y = horizon; y < f.height; ++y) {
            const double d = static_cast<double>(y - horizon) / (f.height - horizon);
            glow(f, f.width / 2 + static_cast<int>(i * 60 * d * 1.6), y, kPink, 0.12 + 0.3 * d);
        }

    // Edge frame and corner arrows: all four must be fully visible.
    neonRect(f, 2, 2, f.width - 4, f.height - 4, kCyan);
    for (int k = 0; k < 12; ++k) {
        for (int j = 0; j <= k; ++j) {
            f.put(6 + j, 6 + k - j, kWhite);
            f.put(f.width - 7 - j, 6 + k - j, kWhite);
            f.put(6 + j, f.height - 7 - k + j, kWhite);
            f.put(f.width - 7 - j, f.height - 7 - k + j, kWhite);
        }
    }

    // Title and status
    drawText(f, 24, 20, "ZEN1", kCyan, 6);
    drawText(f, 128, 32, "ARCADE", kPink, 3);
    drawText(f, 24, 54, "CABINET TEST CARD", kDim, 2);
    drawText(f, 430, 20, info.screenText, kWhite, 1);
    drawText(f, 430, 30, info.planText, kWhite, 1);
    char hz[48];
    std::snprintf(hz, sizeof hz, "REFRESH %.2f HZ", info.measuredHz);
    drawText(f, 430, 40, hz, info.measuredHz > 55 && info.measuredHz < 65 ? kGreen : kAmber, 1);
    std::snprintf(hz, sizeof hz, "FRAME %llu", info.frame);
    drawText(f, 430, 50, hz, kDim, 1);

    // Colour bars and a grey ramp.
    const std::uint32_t bars[] = {kWhite, rgb(255, 220, 0), rgb(0, 220, 220), rgb(0, 200, 0),
                                  rgb(220, 0, 220), rgb(220, 0, 0), rgb(0, 0, 220)};
    for (int i = 0; i < 7; ++i)
        for (int y = 76; y < 92; ++y)
            for (int x = 0; x < 26; ++x) f.put(24 + i * 26 + x, y, bars[i]);
    for (int i = 0; i < 16; ++i)
        for (int y = 76; y < 92; ++y)
            for (int x = 0; x < 12; ++x) f.put(430 + i * 12 + x, y, rgb(i * 17, i * 17, i * 17));

    // Shape check: a circle that is only round at the correct 4:3 shape.
    const int ccx = f.width / 2, ccy = 160, r = 62;
    for (int y = -r - 8; y <= r + 8; ++y)
        for (int x = -r - 8; x <= r + 8; ++x) {
            const double d = std::abs(std::sqrt(x * x + y * y) - r);
            if (d < 1.5) f.put(ccx + x, ccy + y, kWhite);
            else glow(f, ccx + x, ccy + y, kCyan, 0.9 * std::exp(-d / 2.5));
        }
    for (int k = -r + 8; k <= r - 8; ++k) {
        glow(f, ccx + k, ccy, kCyan, 0.5);
        glow(f, ccx, ccy + k, kCyan, 0.5);
    }
    textC(f, ccx, ccy + r + 10, "ROUND = CORRECT SHAPE", kWhite, 1);

    // Motion: the bar must glide without jumps.
    const int track = 246;
    const int barX = static_cast<int>((info.frame * 4) % static_cast<unsigned long long>(f.width + 60)) - 60;
    for (int y = -6; y <= 6; ++y)
        for (int x = -8; x < 68; ++x) {
            const double a = (x >= 0 && x < 60 && std::abs(y) <= 2) ? 1.0 : 0.35 * std::exp(-(std::abs(y) - 2) / 2.0);
            glow(f, barX + x, track + y, kAmber, a);
        }
    drawText(f, 24, 236, "SMOOTH = GOOD", kDim, 1);

    drawPanel(f, 14, 268, info.panels[0], kCyan, "PLAYER 1");
    drawPanel(f, f.width - 314, 268, info.panels[1], kPink, "PLAYER 2");

    // Exit
    textC(f, f.width / 2, 432, "EXIT: HOLD THE 1P BUTTON FOR 5 SECONDS", kDim, 1);
    if (info.exitProgress > 0.0) {
        const int w = static_cast<int>((f.width - 48) * std::min(1.0, info.exitProgress));
        for (int y = 444; y < 452; ++y)
            for (int x = 24; x < 24 + w; ++x) f.put(x, y, kPink);
    }
}

}  // namespace zen1::engine
