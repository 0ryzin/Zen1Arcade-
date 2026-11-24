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

} // namespace zen1::input
