// SPDX-License-Identifier: GPL-3.0-or-later
//
// What each physical input of a panel does. Every cabinet action (the four
// directions, six gameplay buttons, coin, start, the dedicated player button
// and the system functions) has its own binding, so any of them can be
// remapped on its own. The frontend (evdev) and the engine (SDL) both feed a
// PanelSnapshot with the same numbering and resolve it with the same rules.
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace zen1::input {

enum class Action {
    Up, Down, Left, Right,
    B1, B2, B3, B4, B5, B6,   // LP MP HP / LK MK HK
    Coin, Start,
    Player,                   // dedicated 1P / 2P button (gestures)
    VolumeDown, VolumeUp,
    ExitGame, RestartGame,
    Count
};
constexpr int kActionCount = static_cast<int>(Action::Count);

const char* actionKey(Action a);    // "up", "b1", "coin", "volume_down", ...
const char* actionLabel(Action a);  // "UP", "BUTTON 1 (LP)", ...
std::optional<Action> actionFromKey(const std::string& key);
bool isDirection(Action a);
bool isGameplay(Action a);  // directions and B1..B6: reach the game
bool isSystem(Action a);    // player button, volume, exit, restart: never reach the game

struct Bind {
    enum class Kind { None, Stick, Button, AxisNeg, AxisPos, HatUp, HatDown, HatLeft, HatRight };
    Kind kind = Kind::None;
    int index = 0;  // button number, axis number, hat number or stick number

    static Bind none() { return {}; }
    static Bind stick(int n = 0) { return {Kind::Stick, n}; }
    static Bind button(int n) { return {Kind::Button, n}; }
    bool empty() const { return kind == Kind::None; }
    bool operator==(const Bind&) const = default;

    std::string toString() const;               // "button 3", "axis 1-", "hat 0 up", "stick 0", ""
    static Bind parse(const std::string& s);    // inverse; unknown -> none
    std::string label() const;                  // "BUTTON 3", "STICK", "AXIS 1 -", "-" ...
};

struct PanelButtonMap {
    std::array<Bind, kActionCount> bind{};
    PanelButtonMap();  // Zen1 defaults: stick, buttons 0-5, player button 6, the rest unset

    Bind& operator[](Action a) { return bind[static_cast<size_t>(a)]; }
    const Bind& operator[](Action a) const { return bind[static_cast<size_t>(a)]; }
    bool operator==(const PanelButtonMap&) const = default;
};

// Actions (other than `except`) already using `b`. A Stick binding only
// conflicts with another Stick binding of a different direction set, so the
// four default directions never conflict with each other.
std::vector<Action> conflicts(const PanelButtonMap& m, const Bind& b, Action except);

// Raw state of one physical controller.
struct PanelSnapshot {
    std::vector<bool> buttons;            // by button number
    std::vector<double> axes;             // by axis number, -1..1
    std::vector<std::array<int, 2>> hats; // by hat number: x, y in -1..1
    std::array<std::array<int, 2>, 2> stick{};  // calibrated stick 0/1 direction (filled by the reader)
};

bool active(const Bind& b, const PanelSnapshot& s, Action forDirection = Action::Count);

// One player's controls after mapping.
struct Resolved {
    int dirX = 0, dirY = 0;
    std::array<bool, 6> button{};
    bool coin = false, start = false, player = false;
    bool volumeDown = false, volumeUp = false, exitGame = false, restartGame = false;
};
Resolved resolve(const PanelButtonMap& m, const PanelSnapshot& s);

// While capturing a new binding: the input that changed from `before` to
// `now` (a button pressed, an axis pushed past half way, a hat pressed).
std::optional<Bind> detectInput(const PanelSnapshot& before, const PanelSnapshot& now);

}  // namespace zen1::input
