// SPDX-License-Identifier: GPL-3.0-or-later
//
// The cabinet test card: a 640x480 frame shown at 4:3. It checks the
// picture shape, cropping, frame pacing and both panels (see test_card.cpp).
#pragma once

#include "tiny_font.hpp"

#include <array>
#include <string>

namespace zen1::engine {

constexpr int kCardWidth = 640;
constexpr int kCardHeight = 480;
constexpr double kCardAspect = 4.0 / 3.0;

struct PanelState {
    bool connected = false;
    std::string label;             // e.g. "P1 SAVED PORT", "P2 NOT ASSIGNED"
    int dirX = 0, dirY = 0;        // -1, 0, 1
    std::array<bool, 12> buttons{};  // raw button indices 0..11
    std::array<int, 7> map{0, 1, 2, 3, 4, 5, 6};  // LP MP HP LK MK HK player -> raw index
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
