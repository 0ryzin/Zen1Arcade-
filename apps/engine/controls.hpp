// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cabinet controls for the engine: the two arcade panels (assigned by Zen1's
// own rules, never by detection order), plus two development fallbacks that
// are only used for Player 1 while no Player 1 panel is assigned:
//   - the Steam Deck's built-in controls (Valve devices only), and
//   - the keyboard.
// The fallbacks never write anything to controls.toml.
#pragma once

#include "test_card.hpp"

#include "zen1/emu/machine.hpp"
#include "zen1/input/bindings.hpp"
#include "zen1/input/calibration.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <filesystem>
#include <map>
#include <string>

namespace zen1::engine {

// One player's controls in arcade terms, whatever device they came from.
struct LogicalPanel {
    bool connected = false;
    std::string source;              // "PANEL", "STEAM DECK", "KEYBOARD", ""
    int dirX = 0, dirY = 0;
    std::array<bool, 6> action{};    // LP MP HP LK MK HK
    bool player = false;             // dedicated player button (never a gameplay button)
};

class Controls {
public:
    Controls();
    ~Controls();
    Controls(const Controls&) = delete;
    Controls& operator=(const Controls&) = delete;

    void refresh(const std::filesystem::path& controlsFile);
    void handleEvent(const SDL_Event& ev);  // device add/remove, keyboard
    void update();                          // read the current state

    const std::array<PanelState, 2>& raw() const { return raw_; }        // for the test card
    const std::array<LogicalPanel, 2>& logical() const { return logical_; }
    std::string describe(int player) const;

private:
    struct OpenPanel {
        SDL_Joystick* js = nullptr;
        int player = 0;
        input::StickCalibration calibration;
    };
    std::filesystem::path controlsFile_;
    std::map<SDL_JoystickID, OpenPanel> panels_;
    std::map<SDL_JoystickID, SDL_Gamepad*> deckPads_;
    std::array<std::string, 2> reasons_{"NOT ASSIGNED", "NOT ASSIGNED"};
    std::array<input::PanelButtonMap, 2> maps_{};
    std::array<PanelState, 2> raw_{};
    std::array<LogicalPanel, 2> logical_{};
    bool deckFallback_ = true;
    bool keyboardFallback_ = true;
};

// Arcade boards want a coin, then a start. Zen1 panels have one dedicated
// player button: a short press inserts a coin and then presses start.
class CoinStart {
public:
    void trigger() { if (timer_ < 0) timer_ = 0; }
    void step(bool& coin, bool& start);  // once per emulated frame

private:
    int timer_ = -1;
};

// The dedicated player button: short press = coin + start (sent on release,
// so a long hold never reaches the game); held for 5 s = shortcut
// (Player 1: back to the library, Player 2: restart the game). Releasing or
// disconnecting before 5 s cancels.
class PlayerButton {
public:
    struct Result {
        bool shortPress = false;
        bool longHold = false;
        double progress = 0.0;  // 0..1 while held
    };
    Result update(bool down, Uint64 nowNs);
    static constexpr double kHoldSeconds = 5.0;
    static constexpr double kShortSeconds = 0.8;

private:
    bool down_ = false;
    bool fired_ = false;
    Uint64 since_ = 0;
};

// ways: 8, 4 or 2 (game's joystick); prev holds the last restricted directions.
emu::CabinetInput toCabinetInput(const std::array<LogicalPanel, 2>& p, int ways, std::array<int, 4>& prev);

}  // namespace zen1::engine
