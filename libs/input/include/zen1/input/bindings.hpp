// SPDX-License-Identifier: GPL-3.0-or-later
//
// Persistence of Player 1/2 device bindings in Configurations/controls.toml.
#pragma once

#include "zen1/input/assignment.hpp"
#include "zen1/input/mapping.hpp"

#include <array>
#include <filesystem>
#include <string>

namespace zen1::input {

std::array<DeviceIdentity, 2> loadPlayerBindings(const std::filesystem::path& controlsFile,
                                                 std::string* error = nullptr);

// Updates only the [player1]/[player2] device keys; other settings in the
// file are kept. An empty identity is saved as "no device" (player1.none),
// so a panel whose own name says "Player 1" is not picked up again after
// the user unassigned it.
bool savePlayerBindings(const std::filesystem::path& controlsFile,
                        const std::array<DeviceIdentity, 2>& ids,
                        std::string* error = nullptr);

// Saved per player in [player1_map] / [player2_map]: one key per action
// ("up = stick 0", "b1 = button 0", "volume_up = button 9", ...). Older
// files with [playerN_buttons] lp..hk / player keys are still read.
std::array<PanelButtonMap, 2> loadButtonMaps(const std::filesystem::path& controlsFile);
bool saveButtonMaps(const std::filesystem::path& controlsFile, const std::array<PanelButtonMap, 2>& maps,
                    std::string* error = nullptr);

} // namespace zen1::input
