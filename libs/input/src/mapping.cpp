// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/mapping.hpp"

#include <cstddef>
#include <cstdio>
#include <sstream>

namespace zen1::input {

namespace {
struct ActionName {
    const char* key;
    const char* label;
};
constexpr ActionName kNames[kActionCount] = {
    {"up", "UP"},
    {"down", "DOWN"},
    {"left", "LEFT"},
    {"right", "RIGHT"},
    {"b1", "BUTTON 1 (LP)"},
    {"b2", "BUTTON 2 (MP)"},
    {"b3", "BUTTON 3 (HP)"},
    {"b4", "BUTTON 4 (LK)"},
    {"b5", "BUTTON 5 (MK)"},
    {"b6", "BUTTON 6 (HK)"},
    {"coin", "COIN"},
    {"start", "START"},
    {"player", "PLAYER BUTTON"},
    {"volume_down", "VOLUME DOWN"},
    {"volume_up", "VOLUME UP"},
    {"exit_game", "EXIT GAME"},
    {"restart_game", "RESTART GAME"},
};

constexpr const char* kHatDir[4] = {"up", "down", "left", "right"};

int dirOf(Action a) {
    switch (a) {
    case Action::Up: return 0;
    case Action::Down: return 1;
    case Action::Left: return 2;
    case Action::Right: return 3;
    default: return -1;
    }
}
}  // namespace

const char* actionKey(Action a) { return kNames[static_cast<int>(a)].key; }
const char* actionLabel(Action a) { return kNames[static_cast<int>(a)].label; }

std::optional<Action> actionFromKey(const std::string& key) {
    for (int i = 0; i < kActionCount; ++i)
        if (key == kNames[i].key) return static_cast<Action>(i);
    return std::nullopt;
}

bool isDirection(Action a) { return dirOf(a) >= 0; }
bool isGameplay(Action a) { return isDirection(a) || (a >= Action::B1 && a <= Action::B6); }
bool isSystem(Action a) { return a >= Action::Player; }

std::string Bind::toString() const {
    switch (kind) {
    case Kind::None: return "";
    case Kind::Stick: return "stick " + std::to_string(index);
    case Kind::Button: return "button " + std::to_string(index);
    case Kind::AxisNeg: return "axis " + std::to_string(index) + "-";
    case Kind::AxisPos: return "axis " + std::to_string(index) + "+";
    case Kind::HatUp:
    case Kind::HatDown:
    case Kind::HatLeft:
    case Kind::HatRight:
        return "hat " + std::to_string(index) + " " +
               kHatDir[static_cast<int>(kind) - static_cast<int>(Kind::HatUp)];
    }
    return "";
}

Bind Bind::parse(const std::string& s) {
    std::istringstream in(s);
    std::string what;
    in >> what;
    Bind b;
    if (what == "stick" || what == "button") {
        int n = -1;
        if (!(in >> n) || n < 0 || n > 511) return {};
        return {what == "stick" ? Kind::Stick : Kind::Button, n};
    }
    if (what == "axis") {
        std::string rest;
        in >> rest;
        if (rest.size() < 2) return {};
        const char sign = rest.back();
        rest.pop_back();
        int n = -1;
        try { n = std::stoi(rest); } catch (...) { return {}; }
        if (n < 0 || n > 63 || (sign != '-' && sign != '+')) return {};
        return {sign == '-' ? Kind::AxisNeg : Kind::AxisPos, n};
    }
    if (what == "hat") {
        int n = -1;
        std::string dir;
        if (!(in >> n >> dir) || n < 0 || n > 7) return {};
        for (int i = 0; i < 4; ++i)
            if (dir == kHatDir[i]) return {static_cast<Kind>(static_cast<int>(Kind::HatUp) + i), n};
    }
    return b;
}

std::string Bind::label() const {
    char buf[48];
    switch (kind) {
    case Kind::None: return "-";
    case Kind::Stick: return index == 0 ? "STICK" : "STICK " + std::to_string(index + 1);
    case Kind::Button: return "BUTTON " + std::to_string(index);
    case Kind::AxisNeg: std::snprintf(buf, sizeof buf, "AXIS %d -", index); return buf;
    case Kind::AxisPos: std::snprintf(buf, sizeof buf, "AXIS %d +", index); return buf;
    default: {
        static const char* d[4] = {"UP", "DOWN", "LEFT", "RIGHT"};
        std::snprintf(buf, sizeof buf, "HAT %d %s", index, d[static_cast<int>(kind) - static_cast<int>(Kind::HatUp)]);
        return buf;
    }
    }
}

PanelButtonMap::PanelButtonMap() {
    for (Action a : {Action::Up, Action::Down, Action::Left, Action::Right}) (*this)[a] = Bind::stick(0);
    for (int i = 0; i < 6; ++i) bind[static_cast<size_t>(Action::B1) + static_cast<size_t>(i)] = Bind::button(i);
    (*this)[Action::Player] = Bind::button(6);
}

std::vector<Action> conflicts(const PanelButtonMap& m, const Bind& b, Action except) {
    std::vector<Action> out;
    if (b.empty()) return out;
    for (int i = 0; i < kActionCount; ++i) {
        const auto a = static_cast<Action>(i);
        if (a == except) continue;
        const Bind& o = m[a];
        if (o.kind != b.kind || o.index != b.index) continue;
        // One stick drives all four directions by design.
        if (b.kind == Bind::Kind::Stick && isDirection(a) && isDirection(except)) continue;
        out.push_back(a);
    }
    return out;
}

bool active(const Bind& b, const PanelSnapshot& s, Action forDirection) {
    const auto n = static_cast<size_t>(b.index);
    switch (b.kind) {
    case Bind::Kind::None: return false;
    case Bind::Kind::Button: return n < s.buttons.size() && s.buttons[n];
    case Bind::Kind::AxisNeg: return n < s.axes.size() && s.axes[n] < -0.5;
    case Bind::Kind::AxisPos: return n < s.axes.size() && s.axes[n] > 0.5;
    case Bind::Kind::HatUp: return n < s.hats.size() && s.hats[n][1] < 0;
    case Bind::Kind::HatDown: return n < s.hats.size() && s.hats[n][1] > 0;
    case Bind::Kind::HatLeft: return n < s.hats.size() && s.hats[n][0] < 0;
    case Bind::Kind::HatRight: return n < s.hats.size() && s.hats[n][0] > 0;
    case Bind::Kind::Stick: {
        if (n >= s.stick.size()) return false;
        const auto& d = s.stick[n];
        switch (dirOf(forDirection)) {
        case 0: return d[1] < 0;
        case 1: return d[1] > 0;
        case 2: return d[0] < 0;
        case 3: return d[0] > 0;
        default: return d[0] != 0 || d[1] != 0;
        }
    }
    }
    return false;
}

Resolved resolve(const PanelButtonMap& m, const PanelSnapshot& s) {
    Resolved r;
    auto on = [&](Action a) { return active(m[a], s, a); };
    const bool up = on(Action::Up), down = on(Action::Down), left = on(Action::Left), right = on(Action::Right);
    r.dirY = up && !down ? -1 : down && !up ? 1 : 0;
    r.dirX = left && !right ? -1 : right && !left ? 1 : 0;
    for (int i = 0; i < 6; ++i) r.button[static_cast<size_t>(i)] = on(static_cast<Action>(static_cast<int>(Action::B1) + i));
    r.coin = on(Action::Coin);
    r.start = on(Action::Start);
    r.player = on(Action::Player);
    r.volumeDown = on(Action::VolumeDown);
    r.volumeUp = on(Action::VolumeUp);
    r.exitGame = on(Action::ExitGame);
    r.restartGame = on(Action::RestartGame);
    // A physical input bound to a system function never also plays in the game.
    for (int i = 0; i < 6; ++i) {
        const Bind& g = m.bind[static_cast<size_t>(Action::B1) + static_cast<size_t>(i)];
        for (Action sys : {Action::Player, Action::VolumeDown, Action::VolumeUp, Action::ExitGame, Action::RestartGame})
            if (!g.empty() && g == m[sys]) r.button[static_cast<size_t>(i)] = false;
    }
    return r;
}

std::optional<Bind> detectInput(const PanelSnapshot& before, const PanelSnapshot& now) {
    for (size_t i = 0; i < now.buttons.size(); ++i)
        if (now.buttons[i] && !(i < before.buttons.size() && before.buttons[i])) return Bind::button(static_cast<int>(i));
    for (size_t i = 0; i < now.hats.size(); ++i) {
        const std::array<int, 2> prev = i < before.hats.size() ? before.hats[i] : std::array<int, 2>{0, 0};
        const auto& h = now.hats[i];
        const int idx = static_cast<int>(i);
        if (h[1] < 0 && prev[1] >= 0) return Bind{Bind::Kind::HatUp, idx};
        if (h[1] > 0 && prev[1] <= 0) return Bind{Bind::Kind::HatDown, idx};
        if (h[0] < 0 && prev[0] >= 0) return Bind{Bind::Kind::HatLeft, idx};
        if (h[0] > 0 && prev[0] <= 0) return Bind{Bind::Kind::HatRight, idx};
    }
    for (size_t i = 0; i < now.axes.size(); ++i) {
        const double p = i < before.axes.size() ? before.axes[i] : 0.0;
        const int idx = static_cast<int>(i);
        if (now.axes[i] < -0.6 && p >= -0.6) return Bind{Bind::Kind::AxisNeg, idx};
        if (now.axes[i] > 0.6 && p <= 0.6) return Bind{Bind::Kind::AxisPos, idx};
    }
    return std::nullopt;
}

}  // namespace zen1::input
