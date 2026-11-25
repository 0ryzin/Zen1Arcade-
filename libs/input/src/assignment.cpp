// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/assignment.hpp"

#include <algorithm>
#include <cctype>

namespace zen1::input {

DeviceIdentity DeviceIdentity::from(const DeviceInfo& d) {
    return DeviceIdentity{d.byPath, d.vendor, d.product, d.uniq, d.name};
}

int playerFromName(const std::string& name) {
    std::string s;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (s == "player1" || s == "p1" || s == "1p" || s == "playerone") return 1;
    if (s == "player2" || s == "p2" || s == "2p" || s == "playertwo") return 2;
    return 0;
}

const char* toString(MatchReason r) {
    switch (r) {
        case MatchReason::None: return "not assigned";
        case MatchReason::SavedPort: return "saved binding (same USB port)";
        case MatchReason::SavedSerial: return "saved binding (same serial number)";
        case MatchReason::SavedUniqueModel: return "saved binding (only device of that model)";
        case MatchReason::DeviceName: return "device name";
        case MatchReason::Shared: return "same board as the other player";
    }
    return "not assigned";
}

Assignment assignPlayers(const std::vector<DeviceInfo>& devices,
                         const std::array<DeviceIdentity, 2>& saved) {
    Assignment a;
    std::vector<bool> used(devices.size(), false);

    auto sameModel = [](const DeviceInfo& d, const DeviceIdentity& id) {
        return d.vendor == id.vendor && d.product == id.product;
    };
    auto take = [&](int p, size_t i, MatchReason why) {
        a.players[p].device = i;
        a.players[p].reason = why;
        used[i] = true;
    };

    // Pass 1: exact physical port + same model. Strongest evidence.
    for (int p = 0; p < 2; ++p) {
        const auto& id = saved[p];
        if (id.byPath.empty()) continue;
        for (size_t i = 0; i < devices.size(); ++i) {
            if (used[i] || !isPlayerCandidate(devices[i])) continue;
            if (devices[i].byPath == id.byPath && sameModel(devices[i], id)) {
                take(p, i, MatchReason::SavedPort);
                break;
            }
        }
    }

    // Pass 2: same serial number + same model (panel moved to another port).
    for (int p = 0; p < 2; ++p) {
        if (a.players[p].device) continue;
        const auto& id = saved[p];
        if (id.uniq.empty()) continue;
        for (size_t i = 0; i < devices.size(); ++i) {
            if (used[i] || !isPlayerCandidate(devices[i])) continue;
            if (devices[i].uniq == id.uniq && sameModel(devices[i], id)) {
                take(p, i, MatchReason::SavedSerial);
                break;
            }
        }
    }

    // Pass 3: only one connected device of the saved model AND the other
    // player's saved binding is not the same model. Two identical panels
    // without serials can only be told apart by port, so this never guesses
    // between them.
    for (int p = 0; p < 2; ++p) {
        if (a.players[p].device) continue;
        const auto& id = saved[p];
        if (id.empty()) continue;
        const auto& other = saved[1 - p];
        if (!other.empty() && !other.sameDevice(id) && other.vendor == id.vendor && other.product == id.product) continue;
        std::optional<size_t> only;
        int count = 0;
        for (size_t i = 0; i < devices.size(); ++i) {
            if (!isPlayerCandidate(devices[i]) || !sameModel(devices[i], id)) continue;
            ++count;
            if (!used[i]) only = i;
        }
        if (count == 1 && only) take(p, *only, MatchReason::SavedUniqueModel);
    }

    // Pass 4: device name. Only if exactly one free device carries that name,
    // and never for a player the user set to "no device".
    for (int p = 0; p < 2; ++p) {
        if (a.players[p].device || saved[p].none || saved[1 - p].sameDevice(saved[p])) continue;
        std::optional<size_t> match;
        int count = 0;
        for (size_t i = 0; i < devices.size(); ++i) {
            if (used[i] || !isPlayerCandidate(devices[i])) continue;
            if (playerFromName(devices[i].name) == p + 1) {
                ++count;
                match = i;
            }
        }
        if (count == 1) take(p, *match, MatchReason::DeviceName);
    }

    // Shared board: both players saved the same device.
    if (saved[0].sameDevice(saved[1])) {
        for (int p = 0; p < 2; ++p) {
            if (a.players[p].device || !a.players[1 - p].device) continue;
            a.players[p].device = a.players[1 - p].device;
            a.players[p].reason = MatchReason::Shared;
        }
    }

    return a;
}

} // namespace zen1::input
