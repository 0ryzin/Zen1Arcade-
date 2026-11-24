// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/evdev_panel.hpp"

#include <linux/input.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <array>

namespace zen1::input {

namespace {
bool testBit(const unsigned char* bits, int n) { return (bits[n / 8] >> (n % 8)) & 1; }

int threeWay(int v, int lo, int hi) {
    const int span = hi - lo;
    if (span <= 0) return 0;
    if (v < lo + span / 4) return -1;
    if (v > hi - span / 4) return 1;
    return 0;
}
}  // namespace

int PanelLayout::buttonIndex(int code) const {
    const auto it = std::find(buttonCodes.begin(), buttonCodes.end(), code);
    return it == buttonCodes.end() ? -1 : static_cast<int>(it - buttonCodes.begin());
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
    }
    return l;
}

bool PanelEventState::apply(const PanelLayout& l, std::uint16_t type, std::uint16_t code, std::int32_t value) {
    if (buttons.size() != l.buttonCodes.size()) buttons.assign(l.buttonCodes.size(), false);
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
    if (code == ABS_HAT0X) hatX = value < 0 ? -1 : value > 0 ? 1 : 0;
    else if (code == ABS_HAT0Y) hatY = value < 0 ? -1 : value > 0 ? 1 : 0;
    else if (code == ABS_X && l.hasXY) axisX = threeWay(value, l.xMin, l.xMax);
    else if (code == ABS_Y && l.hasXY) axisY = threeWay(value, l.yMin, l.yMax);
    else return false;
    // Same rule as the engine: the hat wins when it is pressed.
    dirX = hatX || hatY ? hatX : axisX;
    dirY = hatX || hatY ? hatY : axisY;
    return dirX != oldX || dirY != oldY;
}

}  // namespace zen1::input
