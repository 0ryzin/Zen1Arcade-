// SPDX-License-Identifier: GPL-3.0-or-later
//
// Machine-readable game lists for the frontend:
//  --list-games  the games the Zen1 engine implements
//  --scan        what is in ~/Zen1Arcade/Games: ready, incomplete or missing
//                ROM sets for supported games, and unsupported files.
#pragma once

#include <filesystem>
#include <string>

namespace zen1::engine {

std::string listGamesJson();
std::string scanJson(const std::filesystem::path& gamesDir);

}  // namespace zen1::engine
