// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads one panel straight from Linux evdev, numbering its buttons exactly
// as SDL does on Linux (BTN_JOYSTICK..KEY_MAX first, then the codes below
// BTN_JOYSTICK, each in ascending order). That way "button 3" means the same
// physical button on the test card, in the remap screen and in a game.
#pragma once

#include "zen1/input/calibration.hpp"

#include <cstdint>
#include <vector>

namespace zen1::input {

struct PanelLayout {
    std::vector<int> buttonCodes;  // index = button number, value = evdev key code
    bool hasHat = false;
    bool hasXY = false;
    int xMin = 0, xMax = 255, yMin = 0, yMax = 255;

    int buttonIndex(int code) const;  // -1 if not a button of this panel
};

// Reads the layout from an open evdev file descriptor.
PanelLayout readPanelLayout(int fd);

// Builds a layout from a key bitmap (for tests): bits[code] = present.
PanelLayout layoutFromKeyBits(const std::vector<bool>& bits);

// Live state of a panel fed with raw events.
struct PanelEventState {
    int dirX = 0, dirY = 0;
    int hatX = 0, hatY = 0, axisX = 0, axisY = 0;
    double nx = 0.0, ny = 0.0;        // stick position from the axes, -1..1 (uncalibrated)
    StickCalibration calibration;     // applied to the axes
    std::vector<bool> buttons;  // by button number

    // Applies one evdev event. Returns true if anything changed.
    bool apply(const PanelLayout& l, std::uint16_t type, std::uint16_t code, std::int32_t value);
};

}  // namespace zen1::input
