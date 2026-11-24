// SPDX-License-Identifier: GPL-3.0-or-later
//
// Persistence of Player 1/2 device bindings in Configurations/controls.toml.
#pragma once

#include "zen1/input/assignment.hpp"

#include <array>
#include <filesystem>
#include <string>

namespace zen1::input {

std::array<DeviceIdentity, 2> loadPlayerBindings(const std::filesystem::path& controlsFile,
                                                 std::string* error = nullptr);

// Updates only the [player1]/[player2] device keys; other settings in the
// file are kept.
bool savePlayerBindings(const std::filesystem::path& controlsFile,
                        const std::array<DeviceIdentity, 2>& ids,
                        std::string* error = nullptr);

// Which raw button index of a panel is which arcade control. Indices are
// the panel's own button numbers (as the test card shows them). -1 = unused.
struct PanelButtonMap {
    // LP MP HP / LK MK HK (Street Fighter layout: top row punches, bottom row kicks)
    std::array<int, 6> action{0, 1, 2, 3, 4, 5};
    int player = 6;  // the 7th "player" button: insert coin + start / join
    bool operator==(const PanelButtonMap&) const = default;
};

std::array<PanelButtonMap, 2> loadButtonMaps(const std::filesystem::path& controlsFile);
bool saveButtonMaps(const std::filesystem::path& controlsFile, const std::array<PanelButtonMap, 2>& maps,
                    std::string* error = nullptr);

} // namespace zen1::input
