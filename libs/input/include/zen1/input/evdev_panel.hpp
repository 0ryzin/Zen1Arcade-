// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads one panel straight from Linux evdev, numbering its buttons exactly
// as SDL does on Linux (BTN_JOYSTICK..KEY_MAX first, then the codes below
// BTN_JOYSTICK, each in ascending order). That way "button 3" means the same
// physical button on the test card, in the remap screen and in a game.
#pragma once

#include "zen1/input/calibration.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace zen1::input {

struct PanelLayout {
    std::vector<int> buttonCodes;  // index = button number, value = evdev key code
    // Axes numbered like SDL on Linux: every ABS code in ascending order
    // except the hats (ABS_HAT0X..ABS_HAT3Y), which become hats 0..3.
    struct Axis { int code, min, max; };
    std::vector<Axis> axes;
    int hatCount = 0;
    bool hasHat = false;
    bool hasXY = false;
    int xMin = 0, xMax = 255, yMin = 0, yMax = 255;

    int buttonIndex(int code) const;  // -1 if not a button of this panel
    int axisIndex(int code) const;    // -1 if not an axis
};

// Reads the layout from an open evdev file descriptor.
PanelLayout readPanelLayout(int fd);

struct PanelSnapshot;
// Snapshot for mapping (zen1/input/mapping.hpp): stick 0 = hat 0 or the
// calibrated X/Y axes, stick 1 = hat 1 or axes 2/3.
PanelSnapshot snapshotOf(const PanelLayout& l, const struct PanelEventState& s);

// Builds a layout from a key bitmap (for tests): bits[code] = present.
PanelLayout layoutFromKeyBits(const std::vector<bool>& bits);

// Live state of a panel fed with raw events.
struct PanelEventState {
    int dirX = 0, dirY = 0;
    int hatX = 0, hatY = 0, axisX = 0, axisY = 0;
    double nx = 0.0, ny = 0.0;        // stick position from the axes, -1..1 (uncalibrated)
    StickCalibration calibration;     // applied to the axes
    std::vector<bool> buttons;  // by button number
    std::vector<double> axes;   // by axis number, -1..1
    std::vector<std::array<int, 2>> hats;

    // Applies one evdev event. Returns true if anything changed.
    bool apply(const PanelLayout& l, std::uint16_t type, std::uint16_t code, std::int32_t value);
};

}  // namespace zen1::input
