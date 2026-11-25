// SPDX-License-Identifier: GPL-3.0-or-later
#include "controls.hpp"

#include "zen1/input/assignment.hpp"
#include "zen1/input/devices.hpp"
#include "zen1/log.hpp"
#include "zen1/settings.hpp"

#include <algorithm>
#include <set>

namespace fs = std::filesystem;

namespace zen1::engine {

namespace {
constexpr int kAxisThreshold = 16000;
constexpr Uint16 kValveVendor = 0x28de;

std::string canonicalOr(const std::string& p) {
    std::error_code ec;
    const auto c = fs::canonical(p, ec);
    return ec ? p : c.string();
}
}  // namespace

Controls::Controls() = default;

Controls::~Controls() {
    for (auto& [id, js] : open_) SDL_CloseJoystick(js);
    for (auto& [id, g] : deckPads_) SDL_CloseGamepad(g);
}

void Controls::refresh(const fs::path& controlsFile) {
    controlsFile_ = controlsFile;
    for (auto& [id, js] : open_) SDL_CloseJoystick(js);
    open_.clear();
    for (auto& [id, g] : deckPads_) SDL_CloseGamepad(g);
    panels_.clear();
    deckPads_.clear();
    reasons_ = {"NOT ASSIGNED", "NOT ASSIGNED"};

    Settings s;
    s.load(controlsFile);
    deckFallback_ = s.getBool("fallback.steam_deck_controls", true);
    keyboardFallback_ = s.getBool("fallback.keyboard", true);
    maps_ = input::loadButtonMaps(controlsFile);

    const auto devices = input::scanDevices();
    const auto a = input::assignPlayers(devices, input::loadPlayerBindings(controlsFile));
    std::multimap<std::string, int> byNode;
    for (int p = 0; p < 2; ++p) {
        if (!a.players[p].device) continue;
        byNode.emplace(canonicalOr(devices[*a.players[p].device].eventPath), p);
        reasons_[p] = a.players[p].reason == input::MatchReason::DeviceName ? "NAME"
                      : a.players[p].reason == input::MatchReason::Shared ? "SHARED BOARD" : "SAVED";
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    std::set<SDL_JoystickID> used;
    for (int i = 0; ids && i < count; ++i) {
        const char* path = SDL_GetJoystickPathForID(ids[i]);
        if (!path) continue;
        const auto range = byNode.equal_range(canonicalOr(path));
        if (range.first == range.second) continue;
        SDL_Joystick* js = SDL_OpenJoystick(ids[i]);
        if (!js) continue;
        open_[ids[i]] = js;
        used.insert(ids[i]);
        for (auto it = range.first; it != range.second; ++it) {
            const auto& dev = devices[*a.players[static_cast<size_t>(it->second)].device];
            panels_.push_back({ids[i], {js, it->second, input::loadCalibration(controlsFile, input::calibrationKey(dev))}});
        }
    }
    // Steam Deck fallback: Valve gamepads only (the built-in controls, or
    // Steam's virtual pad for them). Never an arcade panel.
    if (deckFallback_) {
        for (int i = 0; ids && i < count; ++i) {
            if (used.count(ids[i]) || !SDL_IsGamepad(ids[i])) continue;
            if (SDL_GetJoystickVendorForID(ids[i]) != kValveVendor) continue;
            if (SDL_Gamepad* g = SDL_OpenGamepad(ids[i])) deckPads_[ids[i]] = g;
        }
    }
    SDL_free(ids);
    log::info("controls: P1 " + reasons_[0] + ", P2 " + reasons_[1] + ", Steam Deck pads " +
              std::to_string(deckPads_.size()));
}

void Controls::handleEvent(const SDL_Event& ev) {
    switch (ev.type) {
    case SDL_EVENT_JOYSTICK_ADDED:
    case SDL_EVENT_JOYSTICK_REMOVED:
        refresh(controlsFile_);
        break;
    default:
        break;
    }
}

void Controls::update() {
    for (int p = 0; p < 2; ++p) {
        raw_[p] = PanelState{};
        raw_[p].label = std::string(p == 0 ? "P1 " : "P2 ") + reasons_[p];
        logical_[p] = LogicalPanel{};
    }
    for (const auto& [id, op] : panels_) {
        auto& st = raw_[op.player];
        const auto& m = maps_[op.player];
        input::PanelSnapshot snap;
        snap.buttons.resize(static_cast<size_t>(std::max(0, SDL_GetNumJoystickButtons(op.js))));
        for (size_t b = 0; b < snap.buttons.size(); ++b) snap.buttons[b] = SDL_GetJoystickButton(op.js, static_cast<int>(b));
        snap.axes.resize(static_cast<size_t>(std::max(0, SDL_GetNumJoystickAxes(op.js))));
        for (size_t x = 0; x < snap.axes.size(); ++x)
            snap.axes[x] = std::clamp(SDL_GetJoystickAxis(op.js, static_cast<int>(x)) / 32767.0, -1.0, 1.0);
        snap.hats.resize(static_cast<size_t>(std::max(0, SDL_GetNumJoystickHats(op.js))));
        for (size_t h = 0; h < snap.hats.size(); ++h) {
            const Uint8 v = SDL_GetJoystickHat(op.js, static_cast<int>(h));
            snap.hats[h] = {(v & SDL_HAT_LEFT) ? -1 : (v & SDL_HAT_RIGHT) ? 1 : 0, (v & SDL_HAT_UP) ? -1 : (v & SDL_HAT_DOWN) ? 1 : 0};
        }
        // Stick n: hat n when pressed, else axes 2n / 2n+1 with the saved
        // calibration (centre, travel, dead zone).
        for (size_t n = 0; n < 2; ++n) {
            auto& d = snap.stick[n];
            if (n < snap.hats.size()) d = snap.hats[n];
            if ((d[0] || d[1]) || snap.axes.size() < 2 * n + 2) continue;
            input::stickDirection(op.calibration, snap.axes[2 * n], snap.axes[2 * n + 1], d[0], d[1]);
        }
        const auto r = input::resolve(m, snap);

        st.connected = true;
        for (size_t b = 0; b < st.buttons.size(); ++b) st.buttons[b] = b < snap.buttons.size() && snap.buttons[b];
        st.dirX = r.dirX;
        st.dirY = r.dirY;
        auto rawIndex = [&](input::Action a) { return m[a].kind == input::Bind::Kind::Button ? m[a].index : -1; };
        for (size_t i = 0; i < 6; ++i) st.map[i] = rawIndex(static_cast<input::Action>(static_cast<int>(input::Action::B1) + static_cast<int>(i)));
        st.map[6] = rawIndex(input::Action::Player);

        auto& l = logical_[op.player];
        l.connected = true;
        l.source = "PANEL";
        l.dirX = r.dirX;
        l.dirY = r.dirY;
        l.action = r.button;
        l.player = r.player;
        l.coin = r.coin;
        l.start = r.start;
        l.volumeDown = r.volumeDown;
        l.volumeUp = r.volumeUp;
        l.exitGame = r.exitGame;
        l.restartGame = r.restartGame;
    }

    if (logical_[0].connected) return;  // fallbacks only stand in for a missing Player 1 panel

    auto& l = logical_[0];
    for (const auto& [id, g] : deckPads_) {
        auto b = [&](SDL_GamepadButton x) { return SDL_GetGamepadButton(g, x); };
        const int lx = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX);
        const int ly = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY);
        int dx = b(SDL_GAMEPAD_BUTTON_DPAD_LEFT) ? -1 : b(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? 1 : 0;
        int dy = b(SDL_GAMEPAD_BUTTON_DPAD_UP) ? -1 : b(SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? 1 : 0;
        if (!dx) dx = lx < -kAxisThreshold ? -1 : lx > kAxisThreshold ? 1 : 0;
        if (!dy) dy = ly < -kAxisThreshold ? -1 : ly > kAxisThreshold ? 1 : 0;
        if (dx) l.dirX = dx;
        if (dy) l.dirY = dy;
        // Street Fighter pad layout: X Y RB = punches, A B RT = kicks.
        l.action[0] = l.action[0] || b(SDL_GAMEPAD_BUTTON_WEST);
        l.action[1] = l.action[1] || b(SDL_GAMEPAD_BUTTON_NORTH);
        l.action[2] = l.action[2] || b(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        l.action[3] = l.action[3] || b(SDL_GAMEPAD_BUTTON_SOUTH);
        l.action[4] = l.action[4] || b(SDL_GAMEPAD_BUTTON_EAST);
        l.action[5] = l.action[5] || SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > kAxisThreshold;
        // Start = Player 1 button, View = Player 2 button (when no P2 panel).
        l.player = l.player || b(SDL_GAMEPAD_BUTTON_START);
        if (!logical_[1].connected) logical_[1].player = logical_[1].player || b(SDL_GAMEPAD_BUTTON_BACK);
        l.connected = true;
        l.source = "STEAM DECK";
    }

    if (keyboardFallback_) {
        const bool* k = SDL_GetKeyboardState(nullptr);
        if (k) {
            auto any = [&](std::initializer_list<SDL_Scancode> s) {
                for (auto c : s)
                    if (k[c]) return true;
                return false;
            };
            const int dx = any({SDL_SCANCODE_LEFT}) ? -1 : any({SDL_SCANCODE_RIGHT}) ? 1 : 0;
            const int dy = any({SDL_SCANCODE_UP}) ? -1 : any({SDL_SCANCODE_DOWN}) ? 1 : 0;
            const std::array<bool, 6> act = {k[SDL_SCANCODE_A], k[SDL_SCANCODE_S], k[SDL_SCANCODE_D],
                                             k[SDL_SCANCODE_Z], k[SDL_SCANCODE_X], k[SDL_SCANCODE_C]};
            const bool player = any({SDL_SCANCODE_1, SDL_SCANCODE_RETURN});
            if (!logical_[1].connected) logical_[1].player = logical_[1].player || k[SDL_SCANCODE_2];
            bool used = dx || dy || player;
            for (bool a : act) used = used || a;
            if (dx) l.dirX = dx;
            if (dy) l.dirY = dy;
            for (size_t i = 0; i < 6; ++i) l.action[i] = l.action[i] || act[i];
            l.player = l.player || player;
            if (!l.connected) {
                l.connected = true;
                l.source = "KEYBOARD";
            } else if (used && l.source != "STEAM DECK") {
                l.source = "KEYBOARD";
            }
        }
    }
}

std::string Controls::describe(int player) const {
    const auto& l = logical_[static_cast<size_t>(player)];
    if (!l.connected) return "NO CONTROLS";
    if (l.source == "PANEL") return "PANEL " + reasons_[static_cast<size_t>(player)];
    return l.source + " (STAND-IN, NO P1 PANEL)";
}

void CoinStart::step(bool& coin, bool& start) {
    // Coin for 4 frames, wait, then start for 8 frames (about 0.35 s).
    coin = timer_ >= 0 && timer_ < 4;
    start = timer_ >= 12 && timer_ < 20;
    if (timer_ >= 0 && ++timer_ >= 20) timer_ = -1;
}

emu::CabinetInput toCabinetInput(const std::array<LogicalPanel, 2>& p, int ways, std::array<int, 4>& prev) {
    emu::CabinetInput in;
    for (size_t i = 0; i < 2; ++i) {
        auto& o = in.player[i];
        int dx = p[i].dirX, dy = p[i].dirY;
        input::restrictWays(ways, dx, dy, prev[i * 2], prev[i * 2 + 1]);
        prev[i * 2] = dx;
        prev[i * 2 + 1] = dy;
        o.left = dx < 0;
        o.right = dx > 0;
        o.up = dy < 0;
        o.down = dy > 0;
        o.button = p[i].action;  // the player button is never passed through
    }
    return in;
}

}  // namespace zen1::engine
