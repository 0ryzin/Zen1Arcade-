// SPDX-License-Identifier: GPL-3.0-or-later
//
// zen1-engine: the Zen1 Arcade engine runner.
//
// Phase 2 scope: open fullscreen on the cabinet screen at its native mode
// (no resolution change), present a native-resolution frame through the
// Zen1 display plan, read both panels, talk to the frontend, exit cleanly.
// The frame comes from the built-in test card; machine modules (CPS1) plug
// in at the same point in Phase 4.

#include "ipc_client.hpp"
#include "test_card.hpp"

#include "zen1/data_root.hpp"
#include "zen1/display_plan.hpp"
#include "zen1/input/assignment.hpp"
#include "zen1/input/bindings.hpp"
#include "zen1/input/devices.hpp"
#include "zen1/log.hpp"
#include "zen1/settings.hpp"
#include "zen1/version.hpp"

#include <SDL3/SDL.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace zen1;
using engine::IpcClient;
using engine::PanelState;

namespace {

volatile std::sig_atomic_t g_stop = 0;
void onSignal(int) { g_stop = 1; }

struct Options {
    std::string display;   // substring of the display name, or index
    ScaleMode scale = ScaleMode::Original;
    Rotation rotation = Rotation::R0;
    bool windowed = false;
    std::string ipcPath;
    long long frames = 0;  // >0: exit after N frames (automated tests)
    std::string screenshot;  // with --frames: save the last frame as BMP
    int width = 0, height = 0;  // with --windowed: force a window size (tests)
};

void usage() {
    std::printf(
        "zen1-engine %s\n"
        "Usage: zen1-engine --test-card [options]\n"
        "  --display NAME|INDEX   screen to use (default: saved choice, else 1280x1024, else primary)\n"
        "  --scale MODE           original | integer | pixel-perfect | stretch\n"
        "  --rotate DEG           0 | 90 | 180 | 270\n"
        "  --windowed             window instead of fullscreen (development)\n"
        "  --ipc PATH             frontend socket\n"
        "  --frames N             quit after N frames (tests)\n"
        "  --screenshot FILE.bmp  with --frames: save the final frame\n"
        "  --size WxH             with --windowed: window size (tests)\n",
        kVersion);
}

bool parseArgs(int argc, char** argv, Options& o) {
    bool testCard = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--test-card") testCard = true;
        else if (a == "--display") o.display = next();
        else if (a == "--scale") {
            const auto m = scaleModeFromString(next());
            if (!m) return false;
            o.scale = *m;
        } else if (a == "--rotate") {
            const auto r = rotationFromDegrees(std::atoi(next().c_str()));
            if (!r) return false;
            o.rotation = *r;
        } else if (a == "--windowed") o.windowed = true;
        else if (a == "--ipc") o.ipcPath = next();
        else if (a == "--frames") o.frames = std::atoll(next().c_str());
        else if (a == "--screenshot") o.screenshot = next();
        else if (a == "--size") {
            const std::string s = next();
            if (std::sscanf(s.c_str(), "%dx%d", &o.width, &o.height) != 2) return false;
        }
        else return false;
    }
    return testCard;  // Phase 2: the test card is the only content
}

SDL_DisplayID chooseDisplay(const std::string& wanted) {
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    SDL_DisplayID chosen = 0;
    if (ids) {
        for (int i = 0; i < count && !chosen && !wanted.empty(); ++i) {
            const char* name = SDL_GetDisplayName(ids[i]);
            if (wanted == std::to_string(i) || (name && std::string(name).find(wanted) != std::string::npos))
                chosen = ids[i];
        }
        for (int i = 0; i < count && !chosen; ++i) {
            const SDL_DisplayMode* m = SDL_GetDesktopDisplayMode(ids[i]);
            if (m && m->w == 1280 && m->h == 1024) chosen = ids[i];
        }
        SDL_free(ids);
    }
    return chosen ? chosen : SDL_GetPrimaryDisplay();
}

std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Maps SDL joysticks to Player 1/2 using Zen1's own assignment rules.
class Panels {
public:
    void refresh(const fs::path& controlsFile) {
        for (auto& [id, js] : open_) SDL_CloseJoystick(js);
        open_.clear();
        player_.clear();
        reasons_ = {"NOT ASSIGNED", "NOT ASSIGNED"};

        const auto devices = input::scanDevices();
        const auto saved = input::loadPlayerBindings(controlsFile);
        const auto a = input::assignPlayers(devices, saved);
        std::map<std::string, int> byNode;
        for (int p = 0; p < 2; ++p) {
            if (!a.players[p].device) continue;
            std::error_code ec;
            const auto node = fs::canonical(devices[*a.players[p].device].eventPath, ec);
            byNode[ec ? devices[*a.players[p].device].eventPath : node.string()] = p;
            reasons_[p] = a.players[p].reason == input::MatchReason::DeviceName ? "NAME" : "SAVED";
        }

        int count = 0;
        SDL_JoystickID* ids = SDL_GetJoysticks(&count);
        for (int i = 0; ids && i < count; ++i) {
            const char* path = SDL_GetJoystickPathForID(ids[i]);
            if (!path) continue;
            std::error_code ec;
            const auto node = fs::canonical(path, ec);
            auto it = byNode.find(ec ? std::string(path) : node.string());
            if (it == byNode.end()) continue;
            if (SDL_Joystick* js = SDL_OpenJoystick(ids[i])) {
                open_[ids[i]] = js;
                player_[ids[i]] = it->second;
            }
        }
        SDL_free(ids);
        log::info("panels: P1 " + reasons_[0] + ", P2 " + reasons_[1]);
    }

    void read(std::array<PanelState, 2>& out) const {
        for (int p = 0; p < 2; ++p) {
            out[p] = PanelState{};
            out[p].label = std::string(p == 0 ? "P1 " : "P2 ") + reasons_[p];
        }
        for (const auto& [id, js] : open_) {
            auto& s = out[player_.at(id)];
            s.connected = true;
            const int n = std::min(12, SDL_GetNumJoystickButtons(js));
            for (int b = 0; b < n; ++b) s.buttons[static_cast<size_t>(b)] = SDL_GetJoystickButton(js, b);
            if (SDL_GetNumJoystickHats(js) > 0) {
                const Uint8 h = SDL_GetJoystickHat(js, 0);
                s.dirX = (h & SDL_HAT_LEFT) ? -1 : (h & SDL_HAT_RIGHT) ? 1 : 0;
                s.dirY = (h & SDL_HAT_UP) ? -1 : (h & SDL_HAT_DOWN) ? 1 : 0;
            }
            if (s.dirX == 0 && s.dirY == 0 && SDL_GetNumJoystickAxes(js) >= 2) {
                const int ax = SDL_GetJoystickAxis(js, 0), ay = SDL_GetJoystickAxis(js, 1);
                s.dirX = ax < -16000 ? -1 : ax > 16000 ? 1 : 0;
                s.dirY = ay < -16000 ? -1 : ay > 16000 ? 1 : 0;
            }
        }
    }

    ~Panels() {
        for (auto& [id, js] : open_) SDL_CloseJoystick(js);
    }

private:
    std::map<SDL_JoystickID, SDL_Joystick*> open_;
    std::map<SDL_JoystickID, int> player_;
    std::array<std::string, 2> reasons_{"NOT ASSIGNED", "NOT ASSIGNED"};
};

bool anyButton(const PanelState& p) {
    for (bool b : p.buttons)
        if (b) return true;
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        usage();
        return 2;
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    const DataPaths paths{defaultDataRoot()};
    const auto ensured = ensureDataRoot(paths.root);
    if (ensured.ok) log::openSessionLog(paths.logs(), "engine");
    log::info(std::string("zen1-engine ") + kVersion + " starting");

    Settings display;
    display.load(paths.displayFile());
    if (opt.display.empty()) opt.display = display.getString("cabinet.sdl_display");

    IpcClient ipc;
    if (!opt.ipcPath.empty() && !ipc.connect(opt.ipcPath))
        log::warn("could not connect to frontend at " + opt.ipcPath);

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        log::error(std::string("SDL_Init failed: ") + SDL_GetError());
        ipc.send("error sdl-init");
        return 1;
    }

    const SDL_DisplayID displayId = chooseDisplay(opt.display);
    const SDL_DisplayMode* desk = SDL_GetDesktopDisplayMode(displayId);
    const int deskW = desk ? desk->w : 1280, deskH = desk ? desk->h : 1024;
    const char* dname = SDL_GetDisplayName(displayId);
    log::info("display: " + std::string(dname ? dname : "?") + " " + std::to_string(deskW) + "x" +
              std::to_string(deskH));

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Zen1 Arcade");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(displayId));
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(displayId));
    const int winW = opt.windowed ? (opt.width > 0 ? opt.width : deskW / 2) : deskW;
    const int winH = opt.windowed ? (opt.height > 0 ? opt.height : deskH / 2) : deskH;
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, winW);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, winH);
    // Borderless "desktop" fullscreen: uses the screen's current mode, never switches it.
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, !opt.windowed);
    SDL_Window* window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window) {
        log::error(std::string("window: ") + SDL_GetError());
        ipc.send("error window");
        SDL_Quit();
        return 1;
    }
    SDL_HideCursor();

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        log::error(std::string("renderer: ") + SDL_GetError());
        ipc.send("error renderer");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, engine::kCardWidth, engine::kCardHeight);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    std::vector<std::uint32_t> pixels(static_cast<size_t>(engine::kCardWidth * engine::kCardHeight));
    engine::Frame frame{pixels.data(), engine::kCardWidth, engine::kCardHeight};

    Panels panels;
    panels.refresh(paths.controlsFile());
    ipc.send("ready");

    engine::CardInfo info;
    Uint64 last = SDL_GetTicksNS(), holdStart = 0;
    double avgFrameNs = 1e9 / 60.0;
    std::string exitReason = "quit";
    bool running = true;

    while (running && !g_stop) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = false;
            else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
                exitReason = "keyboard";
                running = false;
            } else if (ev.type == SDL_EVENT_JOYSTICK_ADDED || ev.type == SDL_EVENT_JOYSTICK_REMOVED) {
                panels.refresh(paths.controlsFile());
            }
        }
        while (auto msg = ipc.poll()) {
            if (*msg == "exit") {
                exitReason = "frontend";
                running = false;
            } else if (*msg == "ping") {
                ipc.send("pong");
            }
        }

        int outW = 0, outH = 0;
        SDL_GetRenderOutputSize(renderer, &outW, &outH);
        PlanRequest req;
        req.video = {engine::kCardWidth, engine::kCardHeight, engine::kCardAspect};
        req.screenWidth = outW;
        req.screenHeight = outH;
        req.mode = opt.scale;
        req.rotation = opt.rotation;
        const auto plan = computeDisplayPlan(req);

        panels.read(info.panels);
        const Uint64 now = SDL_GetTicksNS();
        avgFrameNs = avgFrameNs * 0.95 + static_cast<double>(now - last) * 0.05;
        last = now;
        info.measuredHz = avgFrameNs > 0 ? 1e9 / avgFrameNs : 0.0;
        info.screenText = "SCREEN " + std::to_string(outW) + "X" + std::to_string(outH);
        if (plan)
            info.planText = upper(toString(opt.scale)) + " " + std::to_string(plan->game.w) + "X" +
                            std::to_string(plan->game.h);

        // Exit: hold any button on both panels for 3 seconds.
        if (anyButton(info.panels[0]) && anyButton(info.panels[1])) {
            if (!holdStart) holdStart = now;
            info.exitProgress = static_cast<double>(now - holdStart) / 3e9;
            if (info.exitProgress >= 1.0) {
                exitReason = "players";
                running = false;
            }
        } else {
            holdStart = 0;
            info.exitProgress = 0.0;
        }

        engine::drawTestCard(frame, info);
        SDL_UpdateTexture(tex, nullptr, pixels.data(), engine::kCardWidth * 4);

        // Surround layer first (plain colour in Phase 2), game layer last.
        SDL_SetRenderDrawColor(renderer, 6, 6, 10, 255);
        SDL_RenderClear(renderer);
        if (plan) {
            const auto& g = plan->game;
            const bool sideways = opt.rotation == Rotation::R90 || opt.rotation == Rotation::R270;
            // SDL rotates around the rectangle centre: give it the unrotated size.
            const float w = static_cast<float>(sideways ? g.h : g.w);
            const float h = static_cast<float>(sideways ? g.w : g.h);
            const float cx = static_cast<float>(g.x) + static_cast<float>(g.w) / 2.0f;
            const float cy = static_cast<float>(g.y) + static_cast<float>(g.h) / 2.0f;
            const SDL_FRect dst{cx - w / 2.0f, cy - h / 2.0f, w, h};
            SDL_RenderTextureRotated(renderer, tex, nullptr, &dst, static_cast<double>(opt.rotation),
                                     nullptr, SDL_FLIP_NONE);
        }
        ++info.frame;
        if (opt.frames > 0 && static_cast<long long>(info.frame) >= opt.frames) {
            exitReason = "frames";
            running = false;
            if (!opt.screenshot.empty()) {
                if (SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr)) {
                    if (!SDL_SaveBMP(shot, opt.screenshot.c_str()))
                        log::warn(std::string("screenshot failed: ") + SDL_GetError());
                    SDL_DestroySurface(shot);
                }
            }
        }
        SDL_RenderPresent(renderer);
    }

    log::info("exit: " + exitReason);
    ipc.send("exit " + exitReason);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
