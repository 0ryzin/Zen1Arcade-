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
    bool player = false;             // 7th button: coin + start / join
    bool deckExitCombo = false;      // Steam Deck: Start + View held together
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

// Arcade boards want a coin, then a start. Zen1 panels have one "player"
// button, so a press inserts a coin and then presses start.
class AutoCoin {
public:
    // Call once per emulated frame with the player button state.
    void step(bool playerButton, bool& coin, bool& start);

private:
    bool prev_ = false;
    int timer_ = -1;
};

// Leaving a game: hold both player buttons for 2 s, one player button for
// 4 s, or Start + View on the Steam Deck for 2 s.
class ExitHold {
public:
    // Returns progress 0..1; >= 1 means exit now.
    double update(const std::array<LogicalPanel, 2>& p, Uint64 nowNs);

private:
    Uint64 start_ = 0;
};

emu::CabinetInput toCabinetInput(const std::array<LogicalPanel, 2>& p);

}  // namespace zen1::engine
