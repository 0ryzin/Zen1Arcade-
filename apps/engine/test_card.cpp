// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_card.hpp"

#include <cmath>
#include <cstdio>

namespace zen1::engine {
namespace {

constexpr std::uint32_t rgb(int r, int g, int b) {
    return (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) |
           static_cast<std::uint32_t>(b);
}

constexpr std::uint32_t kBg = rgb(16, 18, 28);
constexpr std::uint32_t kGrid = rgb(40, 44, 64);
constexpr std::uint32_t kWhite = rgb(235, 235, 235);
constexpr std::uint32_t kDim = rgb(110, 116, 140);
constexpr std::uint32_t kNeon = rgb(0, 220, 255);
constexpr std::uint32_t kPink = rgb(255, 60, 170);
constexpr std::uint32_t kLit = rgb(255, 210, 40);

void drawPanel(Frame& f, int x0, int y0, const PanelState& p, std::uint32_t accent) {
    drawText(f, x0, y0, p.label, p.connected ? accent : kDim);
    // 8-way stick: 3x3 grid, centre cell marks the stick.
    const int cell = 9, gx = x0, gy = y0 + 9;
    for (int j = -1; j <= 1; ++j)
        for (int i = -1; i <= 1; ++i) {
            const bool on = p.connected && i == p.dirX && j == p.dirY && (i || j);
            const bool centre = i == 0 && j == 0;
            f.fill(gx + (i + 1) * (cell + 1), gy + (j + 1) * (cell + 1), cell, cell,
                   on ? kLit : centre ? kGrid : rgb(30, 34, 50));
        }
    // Buttons 1-12 in two rows of six (arcade layout: top row 1-3 + extras).
    const int bx = x0 + 3 * (cell + 1) + 8;
    for (int b = 0; b < 12; ++b) {
        const int col = b % 6, row = b / 6;
        const int cx = bx + col * 11, cy = gy + row * 15;
        f.fill(cx, cy, 9, 9, p.connected && p.buttons[static_cast<size_t>(b)] ? kLit : rgb(30, 34, 50));
        char num[4];
        std::snprintf(num, sizeof num, "%d", (b + 1) % 10);
        drawText(f, cx + 3, cy + 10, num, kDim);
    }
}

}  // namespace

void drawTestCard(Frame& f, const CardInfo& info) {
    f.fill(0, 0, f.width, f.height, kBg);

    // Grid every 16 native pixels.
    for (int x = 0; x < f.width; x += 16) f.fill(x, 0, 1, f.height, kGrid);
    for (int y = 0; y < f.height; y += 16) f.fill(0, y, f.width, 1, kGrid);

    // Outer frame: all four edges must be visible on the cabinet.
    f.fill(0, 0, f.width, 1, kWhite);
    f.fill(0, f.height - 1, f.width, 1, kWhite);
    f.fill(0, 0, 1, f.height, kWhite);
    f.fill(f.width - 1, 0, 1, f.height, kWhite);

    // Circle that appears round only when shown at the correct 4:3 shape.
    // Native pixels are not square: pixel aspect = (4/3) / (384/224).
    const double pixelAspect = kCardAspect / (static_cast<double>(f.width) / f.height);
    const double ry = 50.0, rx = ry / pixelAspect;
    const double cx = f.width / 2.0, cy = 88.0;
    for (int y = 0; y < f.height; ++y)
        for (int x = 0; x < f.width; ++x) {
            const double dx = (x + 0.5 - cx) / rx, dy = (y + 0.5 - cy) / ry;
            const double d = dx * dx + dy * dy;
            if (d > 0.93 && d < 1.07) f.put(x, y, kNeon);
        }

    // Colour bars.
    const std::uint32_t bars[] = {kWhite, rgb(255, 220, 0), rgb(0, 220, 220), rgb(0, 200, 0),
                                  rgb(220, 0, 220), rgb(220, 0, 0), rgb(0, 0, 220)};
    for (int i = 0; i < 7; ++i) f.fill(8 + i * 10, 8, 10, 6, bars[i]);

    drawText(f, 8, 20, "ZEN1 ARCADE", kNeon, 2);
    drawText(f, 8, 33, "PHASE 2 TEST CARD", kPink);
    drawText(f, f.width - 4 * 22 - 4, 8, info.screenText, kDim);
    drawText(f, f.width - 4 * 22 - 4, 16, info.planText, kDim);
    char hz[32];
    std::snprintf(hz, sizeof hz, "%.2f HZ", info.measuredHz);
    drawText(f, f.width - 4 * 22 - 4, 24, hz, kDim);
    drawText(f, static_cast<int>(cx) - 30, static_cast<int>(cy) - 3, "ROUND = OK", kWhite);

    // Moving bar: should glide without stutter.
    const int barX = static_cast<int>((info.frame * 2) % static_cast<unsigned long long>(f.width + 24)) - 24;
    f.fill(barX, 193, 24, 4, kPink);

    drawPanel(f, 8, 148, info.panels[0], kNeon);
    drawPanel(f, f.width / 2 + 8, 148, info.panels[1], kPink);

    // Exit hint and progress.
    drawText(f, 8, 204, "HOLD A BUTTON ON BOTH PANELS 3 S TO EXIT", kDim);
    if (info.exitProgress > 0.0)
        f.fill(8, 212, static_cast<int>((f.width - 16) * info.exitProgress), 3, kLit);
}

}  // namespace zen1::engine
