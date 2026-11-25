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
        ids[p].none = s.getBool(sec + ".none", false);
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
            if (ids[p].none) s.set(sec + ".none", true);
            else s.remove(sec + ".none");
            continue;
        }
        s.remove(sec + ".none");
        s.set(sec + ".by_path", ids[p].byPath);
        s.set(sec + ".vendor", static_cast<long long>(ids[p].vendor));
        s.set(sec + ".product", static_cast<long long>(ids[p].product));
        s.set(sec + ".serial", ids[p].uniq);
        s.set(sec + ".name", ids[p].name);
    }
    return s.save(file, error);
}

namespace {
constexpr const char* kOldKeys[6] = {"lp", "mp", "hp", "lk", "mk", "hk"};
}

std::array<PanelButtonMap, 2> loadButtonMaps(const std::filesystem::path& file) {
    Settings s;
    s.load(file);
    std::array<PanelButtonMap, 2> maps;
    for (int p = 0; p < 2; ++p) {
        auto& m = maps[static_cast<size_t>(p)];
        // Older format: button numbers only.
        const std::string old = std::string(section(p)) + "_buttons.";
        for (int i = 0; i < 6; ++i) {
            const long long v = s.getInt(old + kOldKeys[i], -2);
            if (v >= 0) m.bind[static_cast<size_t>(Action::B1) + static_cast<size_t>(i)] = Bind::button(static_cast<int>(v));
            else if (v == -1) m.bind[static_cast<size_t>(Action::B1) + static_cast<size_t>(i)] = Bind::none();
        }
        if (const long long v = s.getInt(old + "player", -2); v >= -1)
            m[Action::Player] = v >= 0 ? Bind::button(static_cast<int>(v)) : Bind::none();
        // Current format: one binding per action.
        const std::string sec = std::string(section(p)) + "_map.";
        for (int i = 0; i < kActionCount; ++i) {
            const auto a = static_cast<Action>(i);
            if (!s.has(sec + actionKey(a))) continue;
            m[a] = Bind::parse(s.getString(sec + actionKey(a)));
        }
    }
    return maps;
}

bool saveButtonMaps(const std::filesystem::path& file, const std::array<PanelButtonMap, 2>& maps,
                    std::string* error) {
    Settings s;
    if (!s.load(file, error)) return false;
    for (int p = 0; p < 2; ++p) {
        const std::string old = std::string(section(p)) + "_buttons.";
        for (const char* k : kOldKeys) s.remove(old + k);
        s.remove(old + "player");
        const std::string sec = std::string(section(p)) + "_map.";
        for (int i = 0; i < kActionCount; ++i) {
            const auto a = static_cast<Action>(i);
            s.set(sec + actionKey(a), maps[static_cast<size_t>(p)][a].toString());
        }
    }
    return s.save(file, error);
}

} // namespace zen1::input
