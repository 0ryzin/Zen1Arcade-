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
    for (auto& [id, p] : panels_) SDL_CloseJoystick(p.js);
    for (auto& [id, g] : deckPads_) SDL_CloseGamepad(g);
}

void Controls::refresh(const fs::path& controlsFile) {
    controlsFile_ = controlsFile;
    for (auto& [id, p] : panels_) SDL_CloseJoystick(p.js);
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
    std::map<std::string, int> byNode;
    for (int p = 0; p < 2; ++p) {
        if (!a.players[p].device) continue;
        byNode[canonicalOr(devices[*a.players[p].device].eventPath)] = p;
        reasons_[p] = a.players[p].reason == input::MatchReason::DeviceName ? "NAME" : "SAVED";
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    std::set<SDL_JoystickID> used;
    for (int i = 0; ids && i < count; ++i) {
        const char* path = SDL_GetJoystickPathForID(ids[i]);
        if (!path) continue;
        auto it = byNode.find(canonicalOr(path));
        if (it == byNode.end()) continue;
        if (SDL_Joystick* js = SDL_OpenJoystick(ids[i])) {
            const auto& dev = devices[*a.players[static_cast<size_t>(it->second)].device];
            panels_[ids[i]] = {js, it->second, input::loadCalibration(controlsFile, input::calibrationKey(dev))};
            used.insert(ids[i]);
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
        auto& s = raw_[op.player];
        s.connected = true;
        const int n = std::min(12, SDL_GetNumJoystickButtons(op.js));
        for (int b = 0; b < n; ++b) s.buttons[static_cast<size_t>(b)] = SDL_GetJoystickButton(op.js, b);
        if (SDL_GetNumJoystickHats(op.js) > 0) {
            const Uint8 h = SDL_GetJoystickHat(op.js, 0);
            s.dirX = (h & SDL_HAT_LEFT) ? -1 : (h & SDL_HAT_RIGHT) ? 1 : 0;
            s.dirY = (h & SDL_HAT_UP) ? -1 : (h & SDL_HAT_DOWN) ? 1 : 0;
        }
        if (s.dirX == 0 && s.dirY == 0 && SDL_GetNumJoystickAxes(op.js) >= 2) {
            // SDL scales the axes to the device's reported range; the saved
            // calibration (centre, travel, dead zone) is applied on top.
            const double nx = SDL_GetJoystickAxis(op.js, 0) / 32767.0, ny = SDL_GetJoystickAxis(op.js, 1) / 32767.0;
            input::stickDirection(op.calibration, std::clamp(nx, -1.0, 1.0), std::clamp(ny, -1.0, 1.0), s.dirX, s.dirY);
        }
        auto& l = logical_[op.player];
        const auto& m = maps_[op.player];
        for (size_t i = 0; i < 6; ++i) s.map[i] = m.action[i];
        s.map[6] = m.player;
        auto btn = [&](int i) { return i >= 0 && i < 12 && s.buttons[static_cast<size_t>(i)]; };
        l.connected = true;
        l.source = "PANEL";
        l.dirX = s.dirX;
        l.dirY = s.dirY;
        for (int i = 0; i < 6; ++i) l.action[static_cast<size_t>(i)] = btn(m.action[static_cast<size_t>(i)]);
        l.player = btn(m.player);
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

PlayerButton::Result PlayerButton::update(bool down, Uint64 nowNs) {
    Result r;
    if (down && !down_) {
        since_ = nowNs;
        fired_ = false;
    }
    const double held = down_ || down ? static_cast<double>(nowNs - since_) / 1e9 : 0.0;
    if (!down && down_ && !fired_ && held < kShortSeconds) r.shortPress = true;
    if (down && !fired_) {
        r.progress = std::min(1.0, held / kHoldSeconds);
        if (held >= kHoldSeconds) {
            r.longHold = true;
            fired_ = true;
        }
    }
    down_ = down;
    return r;
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
