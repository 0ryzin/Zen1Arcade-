// SPDX-License-Identifier: GPL-3.0-or-later
//
// Player 1 / Player 2 assignment. Never based on detection order:
//   1. a saved binding for that exact device (physical port, IDs, serial)
//   2. the device name ("Player 1", "Player 2", "P1", ...)
//   3. otherwise unassigned -> the frontend runs the one-time wizard
#pragma once

#include "zen1/input/devices.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace zen1::input {

struct DeviceIdentity {
    std::string byPath;
    std::uint16_t vendor = 0;
    std::uint16_t product = 0;
    std::string uniq;
    std::string name;

    static DeviceIdentity from(const DeviceInfo& d);
    bool empty() const { return byPath.empty() && uniq.empty() && vendor == 0 && product == 0; }
};

enum class MatchReason { None, SavedPort, SavedSerial, SavedUniqueModel, DeviceName };

struct PlayerSlot {
    std::optional<size_t> device;  // index into the scanned device list
    MatchReason reason = MatchReason::None;
};

struct Assignment {
    std::array<PlayerSlot, 2> players;  // [0] = Player 1, [1] = Player 2
    bool complete() const { return players[0].device && players[1].device; }
};

// saved[0] / saved[1] are the stored identities for Player 1 / Player 2
// (empty identity = nothing stored).
Assignment assignPlayers(const std::vector<DeviceInfo>& devices,
                         const std::array<DeviceIdentity, 2>& saved);

// 1 or 2 if the name clearly means a player ("Player 1", "player2", "P1",
// "1P"), otherwise 0.
int playerFromName(const std::string& name);

const char* toString(MatchReason r);

} // namespace zen1::input
