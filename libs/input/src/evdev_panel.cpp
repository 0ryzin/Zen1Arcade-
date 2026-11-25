// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/evdev_panel.hpp"

#include <linux/input.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "zen1/input/mapping.hpp"

namespace zen1::input {

namespace {
bool testBit(const unsigned char* bits, int n) { return (bits[n / 8] >> (n % 8)) & 1; }

}  // namespace

int PanelLayout::buttonIndex(int code) const {
    const auto it = std::find(buttonCodes.begin(), buttonCodes.end(), code);
    return it == buttonCodes.end() ? -1 : static_cast<int>(it - buttonCodes.begin());
}

int PanelLayout::axisIndex(int code) const {
    for (size_t i = 0; i < axes.size(); ++i)
        if (axes[i].code == code) return static_cast<int>(i);
    return -1;
}

PanelLayout layoutFromKeyBits(const std::vector<bool>& bits) {
    PanelLayout l;
    const int n = static_cast<int>(bits.size());
    for (int i = BTN_JOYSTICK; i < std::min(n, KEY_MAX); ++i)
        if (bits[static_cast<size_t>(i)]) l.buttonCodes.push_back(i);
    for (int i = 0; i < std::min(n, BTN_JOYSTICK); ++i)
        if (bits[static_cast<size_t>(i)]) l.buttonCodes.push_back(i);
    return l;
}

PanelLayout readPanelLayout(int fd) {
    std::array<unsigned char, KEY_MAX / 8 + 1> keys{};
    std::array<unsigned char, ABS_MAX / 8 + 1> abs{};
    PanelLayout l;
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys.data()) >= 0) {
        std::vector<bool> bits(KEY_MAX, false);
        for (int i = 0; i < KEY_MAX; ++i) bits[static_cast<size_t>(i)] = testBit(keys.data(), i);
        l = layoutFromKeyBits(bits);
    }
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof abs), abs.data()) >= 0) {
        l.hasHat = testBit(abs.data(), ABS_HAT0X) && testBit(abs.data(), ABS_HAT0Y);
        l.hasXY = testBit(abs.data(), ABS_X) && testBit(abs.data(), ABS_Y);
        input_absinfo ai{};
        if (l.hasXY && ioctl(fd, EVIOCGABS(ABS_X), &ai) >= 0) { l.xMin = ai.minimum; l.xMax = ai.maximum; }
        if (l.hasXY && ioctl(fd, EVIOCGABS(ABS_Y), &ai) >= 0) { l.yMin = ai.minimum; l.yMax = ai.maximum; }
        for (int c = 0; c < ABS_MISC; ++c) {
            if (!testBit(abs.data(), c)) continue;
            if (c >= ABS_HAT0X && c <= ABS_HAT3Y) continue;
            input_absinfo a{};
            if (ioctl(fd, EVIOCGABS(c), &a) < 0) continue;
            l.axes.push_back({c, a.minimum, a.maximum});
        }
        for (int h = 0; h < 4; ++h)
            if (testBit(abs.data(), ABS_HAT0X + 2 * h) && testBit(abs.data(), ABS_HAT0Y + 2 * h)) l.hatCount = h + 1;
    }
    return l;
}

bool PanelEventState::apply(const PanelLayout& l, std::uint16_t type, std::uint16_t code, std::int32_t value) {
    if (buttons.size() != l.buttonCodes.size()) buttons.assign(l.buttonCodes.size(), false);
    if (axes.size() != l.axes.size()) axes.assign(l.axes.size(), 0.0);
    if (hats.size() != static_cast<size_t>(l.hatCount)) hats.assign(static_cast<size_t>(l.hatCount), {0, 0});
    const int oldX = dirX, oldY = dirY;
    if (type == EV_KEY) {
        const int i = l.buttonIndex(code);
        if (i < 0 || value == 2) return false;  // unknown, or key repeat
        const bool down = value != 0;
        if (buttons[static_cast<size_t>(i)] == down) return false;
        buttons[static_cast<size_t>(i)] = down;
        return true;
    }
    if (type != EV_ABS) return false;
    bool other = false;
    if (code >= ABS_HAT0X && code <= ABS_HAT3Y) {
        const size_t h = static_cast<size_t>((code - ABS_HAT0X) / 2);
        if (h < hats.size()) {
            auto& v = hats[h][(code - ABS_HAT0X) % 2];
            const int n = value < 0 ? -1 : value > 0 ? 1 : 0;
            other = v != n;
            v = n;
        }
    } else if (const int ai = l.axisIndex(code); ai >= 0) {
        const auto& a = l.axes[static_cast<size_t>(ai)];
        const double n = normalizeAxis(value, a.min, a.max);
        const double old = axes[static_cast<size_t>(ai)];
        axes[static_cast<size_t>(ai)] = n;
        other = (std::abs(n) > 0.5) != (std::abs(old) > 0.5) || (n > 0) != (old > 0);
    }
    if (code == ABS_HAT0X) hatX = value < 0 ? -1 : value > 0 ? 1 : 0;
    else if (code == ABS_HAT0Y) hatY = value < 0 ? -1 : value > 0 ? 1 : 0;
    else if ((code == ABS_X || code == ABS_Y) && l.hasXY) {
        if (code == ABS_X) nx = normalizeAxis(value, l.xMin, l.xMax);
        else ny = normalizeAxis(value, l.yMin, l.yMax);
        stickDirection(calibration, nx, ny, axisX, axisY);
    } else {
        return other;
    }
    // Same rule as the engine: the hat wins when it is pressed.
    dirX = hatX || hatY ? hatX : axisX;
    dirY = hatX || hatY ? hatY : axisY;
    return dirX != oldX || dirY != oldY || other;
}

PanelSnapshot snapshotOf(const PanelLayout& l, const PanelEventState& s) {
    PanelSnapshot p;
    p.buttons = s.buttons;
    p.axes = s.axes;
    p.hats = s.hats;
    p.stick[0] = {s.dirX, s.dirY};
    if (p.hats.size() > 1) p.stick[1] = p.hats[1];
    if (!p.stick[1][0] && !p.stick[1][1] && p.axes.size() >= 4) {
        StickCalibration c;
        stickDirection(c, p.axes[2], p.axes[3], p.stick[1][0], p.stick[1][1]);
    }
    (void)l;
    return p;
}

}  // namespace zen1::input
