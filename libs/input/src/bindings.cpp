// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/bindings.hpp"

#include "zen1/settings.hpp"

namespace zen1::input {
namespace {
const char* section(int p) { return p == 0 ? "player1" : "player2"; }
}

std::array<DeviceIdentity, 2> loadPlayerBindings(const std::filesystem::path& file,
                                                 std::string* error) {
    Settings s;
    s.load(file, error);
    std::array<DeviceIdentity, 2> ids;
    for (int p = 0; p < 2; ++p) {
        const std::string sec = section(p);
        ids[p].byPath = s.getString(sec + ".by_path");
        ids[p].vendor = static_cast<std::uint16_t>(s.getInt(sec + ".vendor"));
        ids[p].product = static_cast<std::uint16_t>(s.getInt(sec + ".product"));
        ids[p].uniq = s.getString(sec + ".serial");
        ids[p].name = s.getString(sec + ".name");
    }
    return ids;
}

bool savePlayerBindings(const std::filesystem::path& file,
                        const std::array<DeviceIdentity, 2>& ids, std::string* error) {
    Settings s;
    if (!s.load(file, error)) return false;
    for (int p = 0; p < 2; ++p) {
        const std::string sec = section(p);
        if (ids[p].empty()) {
            for (const char* k : {"by_path", "vendor", "product", "serial", "name"}) s.remove(sec + "." + k);
            continue;
        }
        s.set(sec + ".by_path", ids[p].byPath);
        s.set(sec + ".vendor", static_cast<long long>(ids[p].vendor));
        s.set(sec + ".product", static_cast<long long>(ids[p].product));
        s.set(sec + ".serial", ids[p].uniq);
        s.set(sec + ".name", ids[p].name);
    }
    return s.save(file, error);
}

namespace {
constexpr const char* kActionKeys[6] = {"lp", "mp", "hp", "lk", "mk", "hk"};
}

std::array<PanelButtonMap, 2> loadButtonMaps(const std::filesystem::path& file) {
    Settings s;
    s.load(file);
    std::array<PanelButtonMap, 2> maps;
    for (int p = 0; p < 2; ++p) {
        const std::string sec = std::string(section(p)) + "_buttons.";
        for (int i = 0; i < 6; ++i)
            maps[p].action[i] = static_cast<int>(s.getInt(sec + kActionKeys[i], maps[p].action[i]));
        maps[p].player = static_cast<int>(s.getInt(sec + "player", maps[p].player));
    }
    return maps;
}

bool saveButtonMaps(const std::filesystem::path& file, const std::array<PanelButtonMap, 2>& maps,
                    std::string* error) {
    Settings s;
    if (!s.load(file, error)) return false;
    for (int p = 0; p < 2; ++p) {
        const std::string sec = std::string(section(p)) + "_buttons.";
        for (int i = 0; i < 6; ++i) s.set(sec + kActionKeys[i], static_cast<long long>(maps[p].action[i]));
        s.set(sec + "player", static_cast<long long>(maps[p].player));
    }
    return s.save(file, error);
}

} // namespace zen1::input
