// SPDX-License-Identifier: GPL-3.0-or-later
//
// Discovery of physical input devices through Linux evdev.
// Read-only: devices are opened non-exclusively and never grabbed.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zen1::input {

enum class DeviceKind { Joystick, Gamepad, Keyboard, Other };

struct DeviceInfo {
    std::string eventPath;   // /dev/input/eventN (changes between boots)
    std::string byPath;      // /dev/input/by-path/... (stable per physical port), may be empty
    std::string name;        // name reported by the device (USB descriptor)
    std::string phys;        // kernel physical location string
    std::string uniq;        // serial number, often empty on arcade encoders
    std::uint16_t bustype = 0;
    std::uint16_t vendor = 0;
    std::uint16_t product = 0;
    std::uint16_t version = 0;
    DeviceKind kind = DeviceKind::Other;
    int buttonCount = 0;     // BTN_* joystick/gamepad buttons
    int axisCount = 0;       // ABS_* axes
    bool hasHat = false;     // ABS_HAT0X/Y (common for 8-way sticks)
    bool virtualDevice = false; // created in software (uinput), e.g. Steam's virtual pads
    bool valve = false;         // Valve hardware, e.g. the Steam Deck's own controls
    bool readable = true;    // false if permissions prevented opening
};

// Scans /dev/input/event*. `devRoot` is overridable for tests.
std::vector<DeviceInfo> scanDevices(const std::string& devRoot = "/dev/input");

// True for devices Zen1 should consider for Player 1/2.
bool isPlayerCandidate(const DeviceInfo& d);

const char* toString(DeviceKind k);
std::string hex4(std::uint16_t v);

} // namespace zen1::input
