// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/devices.hpp"

#include <cstdlib>
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <map>

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace zen1::input {
namespace {

constexpr std::uint16_t kValveVendor = 0x28de;

template <size_t N>
bool testBit(const std::array<unsigned long, N>& bits, unsigned bit) {
    constexpr unsigned kBitsPerLong = sizeof(unsigned long) * 8;
    if (bit / kBitsPerLong >= N) return false;
    return (bits[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1UL;
}

std::string ioctlString(int fd, unsigned long req) {
    char buf[256] = {};
    if (ioctl(fd, req, buf) < 0) return {};
    buf[sizeof buf - 1] = '\0';
    return buf;
}

// Maps /dev/input/eventN -> /dev/input/by-path/... symlink.
std::map<std::string, std::string> byPathLinks(const std::string& devRoot) {
    std::map<std::string, std::string> out;
    std::error_code ec;
    const fs::path dir = fs::path(devRoot) / "by-path";
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        const std::string n = e.path().filename().string();
        if (n.find("event") == std::string::npos) continue;
        const fs::path target = fs::canonical(e.path(), ec);
        if (ec) continue;
        // Prefer the "-event-joystick" link when several exist for one node.
        auto& slot = out[target.string()];
        if (slot.empty() || n.find("event-joystick") != std::string::npos) slot = e.path().string();
    }
    return out;
}

} // namespace

std::string hex4(std::uint16_t v) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "%04x", v);
    return buf;
}

const char* toString(DeviceKind k) {
    switch (k) {
        case DeviceKind::Joystick: return "joystick";
        case DeviceKind::Gamepad: return "gamepad";
        case DeviceKind::Keyboard: return "keyboard";
        case DeviceKind::Other: return "other";
    }
    return "other";
}

bool isPlayerCandidate(const DeviceInfo& d) {
    if (d.virtualDevice || d.valve) return false;
    return d.kind == DeviceKind::Joystick || d.kind == DeviceKind::Gamepad;
}

std::vector<DeviceInfo> scanDevices(const std::string& devRoot) {
    std::vector<DeviceInfo> out;
    std::error_code ec;
    const auto links = byPathLinks(devRoot);

    std::vector<fs::path> nodes;
    for (const auto& e : fs::directory_iterator(devRoot, ec)) {
        if (e.path().filename().string().rfind("event", 0) == 0) nodes.push_back(e.path());
    }
    // Natural order: event2 before event10.
    std::sort(nodes.begin(), nodes.end(), [](const fs::path& a, const fs::path& b) {
        const auto na = a.filename().string(), nb = b.filename().string();
        return na.size() != nb.size() ? na.size() < nb.size() : na < nb;
    });

    for (const auto& node : nodes) {
        DeviceInfo d;
        d.eventPath = node.string();
        const fs::path canon = fs::canonical(node, ec);
        if (auto it = links.find(ec ? d.eventPath : canon.string()); it != links.end())
            d.byPath = it->second;

        const int fd = ::open(d.eventPath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            d.readable = false;
            out.push_back(std::move(d));
            continue;
        }

        d.name = ioctlString(fd, EVIOCGNAME(255));
        d.phys = ioctlString(fd, EVIOCGPHYS(255));
        d.uniq = ioctlString(fd, EVIOCGUNIQ(255));

        input_id id{};
        if (ioctl(fd, EVIOCGID, &id) == 0) {
            d.bustype = id.bustype;
            d.vendor = id.vendor;
            d.product = id.product;
            d.version = id.version;
        }

        std::array<unsigned long, (KEY_MAX / (sizeof(unsigned long) * 8)) + 1> keys{};
        std::array<unsigned long, (ABS_MAX / (sizeof(unsigned long) * 8)) + 1> abs{};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys.data());
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof abs), abs.data());
        ::close(fd);

        bool joystickButtons = false, gamepadButtons = false;
        for (unsigned b = BTN_JOYSTICK; b < BTN_JOYSTICK + 16; ++b) {
            if (testBit(keys, b)) { ++d.buttonCount; joystickButtons = true; }
        }
        for (unsigned b = BTN_GAMEPAD; b < BTN_GAMEPAD + 16; ++b) {
            if (testBit(keys, b)) { ++d.buttonCount; gamepadButtons = true; }
        }
        for (unsigned b = BTN_TRIGGER_HAPPY; b < BTN_TRIGGER_HAPPY + 40; ++b) {
            if (testBit(keys, b)) ++d.buttonCount;
        }
        for (unsigned a = 0; a < ABS_MAX; ++a) {
            if (testBit(abs, a)) ++d.axisCount;
        }
        d.hasHat = testBit(abs, ABS_HAT0X) && testBit(abs, ABS_HAT0Y);

        const bool keyboardKeys = testBit(keys, KEY_A) && testBit(keys, KEY_Z) && testBit(keys, KEY_ENTER);
        if (gamepadButtons) d.kind = DeviceKind::Gamepad;
        else if (joystickButtons) d.kind = DeviceKind::Joystick;
        else if (keyboardKeys) d.kind = DeviceKind::Keyboard;
        else d.kind = DeviceKind::Other;

        d.valve = d.vendor == kValveVendor;
        // Software devices (uinput) live under /sys/devices/virtual/. Steam's
        // virtual gamepads are created this way; physical USB panels are not.
        const fs::path sysDev =
            fs::path("/sys/class/input") / node.filename() / "device";
        const fs::path real = fs::canonical(sysDev, ec);
        d.virtualDevice = (!ec && real.string().find("/devices/virtual/") != std::string::npos) ||
                          d.name.find("Steam Virtual") != std::string::npos;
        // Tests only: a uinput device that stands in for a physical panel
        // (CI has no USB hardware). Never set in normal use.
        if (d.virtualDevice && d.name.rfind("Zen1 Test ", 0) == 0 && std::getenv("ZEN1_TEST_UINPUT_PANELS"))
            d.virtualDevice = false;

        out.push_back(std::move(d));
    }
    return out;
}

} // namespace zen1::input
