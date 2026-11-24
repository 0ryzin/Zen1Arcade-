// SPDX-License-Identifier: GPL-3.0-or-later
//
// Read-only view of the GPU's display connectors via /sys/class/drm.
// Works under Wayland, X11 or no desktop, needs no permissions, and never
// changes any display setting.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zen1::display {

struct Mode {
    int width = 0;
    int height = 0;
};

struct EdidInfo {
    std::string manufacturer;  // 3-letter PNP id, e.g. "SAM"
    std::uint16_t productCode = 0;
    std::uint32_t serial = 0;
    std::string monitorName;   // descriptor 0xFC, may be empty
    int widthCm = 0;           // physical size, 0 if unknown
    int heightCm = 0;
    std::optional<Mode> preferred;  // first detailed timing
};

struct Output {
    std::string connector;     // e.g. "card1-DP-1", "card1-eDP-1"
    std::string status;        // "connected", "disconnected", ...
    bool enabled = false;
    bool internal = false;     // built-in panel (eDP/LVDS/DSI), e.g. the Deck's own screen
    std::vector<Mode> modes;   // as listed by the kernel, preferred first
    std::optional<EdidInfo> edid;
};

std::vector<Output> scanOutputs(const std::string& drmRoot = "/sys/class/drm");

// Parses a 128+ byte EDID block. nullopt if the header/checksum is invalid.
std::optional<EdidInfo> parseEdid(const std::vector<std::uint8_t>& bytes);

// Picks the cabinet screen: a connected external output whose preferred
// mode is 1280x1024 if there is one, otherwise the first connected external
// output, otherwise the first connected output.
std::optional<size_t> guessCabinetOutput(const std::vector<Output>& outputs);

} // namespace zen1::display
