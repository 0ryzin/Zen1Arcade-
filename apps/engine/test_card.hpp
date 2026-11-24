// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Phase 2 test card: a synthetic 384x224 "game" frame shown at 4:3,
// exactly like a CPS1 game. It checks the cabinet setup before any
// emulation exists:
//  - the circle must look round on the cabinet (correct aspect ratio)
//  - the white frame must be fully visible (nothing cropped)
//  - the moving bar must glide smoothly (frame pacing)
//  - each panel's stick and buttons light up live (Player 1 left, Player 2 right)
#pragma once

#include "tiny_font.hpp"

#include <array>
#include <string>

namespace zen1::engine {

constexpr int kCardWidth = 384;
constexpr int kCardHeight = 224;
constexpr double kCardAspect = 4.0 / 3.0;

struct PanelState {
    bool connected = false;
    std::string label;             // e.g. "P1 SAVED PORT", "P2 NOT ASSIGNED"
    int dirX = 0, dirY = 0;        // -1, 0, 1
    std::array<bool, 12> buttons{};  // raw button indices 0..11
};

struct CardInfo {
    unsigned long long frame = 0;
    double measuredHz = 0.0;
    std::string planText;          // e.g. "1280X960 AT 0,32"
    std::string screenText;        // e.g. "SCREEN 1280X1024"
    std::array<PanelState, 2> panels;
    double exitProgress = 0.0;     // 0..1 while the exit hold is in progress
};

void drawTestCard(Frame& f, const CardInfo& info);

}  // namespace zen1::engine
