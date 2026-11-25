// SPDX-License-Identifier: GPL-3.0-or-later
//
// zen1-engine: the Zen1 Arcade engine runner.
//
// Runs one of Zen1's own machine modules (or the cabinet test card)
// fullscreen on the cabinet screen at its current mode (the resolution is
// never changed), presents the frame through the Zen1 display plan with a
// surround, plays audio, maps both panels, and talks to the frontend.

#include "controls.hpp"
#include "cps1_selftest.hpp"
#include "ipc_client.hpp"
#include "library_scan.hpp"
#include "consoles.hpp"
#include "presenter.hpp"
#include "test_card.hpp"

#include "zen1/data_root.hpp"
#include "zen1/input/gesture.hpp"

#include <fstream>
#include <iterator>
#include "zen1/display_plan.hpp"
#include "zen1/emu/games.hpp"
#include "zen1/input/calibration.hpp"
#include "zen1/log.hpp"
#include "zen1/settings.hpp"
#include "zen1/version.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace zen1;
using namespace zen1::engine;

namespace {

volatile std::sig_atomic_t g_stop = 0;
void onSignal(int) { g_stop = 1; }

enum class Mode { None, TestCard, Game, SelfTest, ListGames, Scan };

struct Options {
    std::string zip;
    std::string rom;       // console cartridge file (--rom)
    Mode mode = Mode::None;
    std::string game;
    std::string gamesDir;
    std::string display;   // substring of the display name, or index
    std::string scale;     // empty: saved setting
    int rotate = -1;       // extra rotation on top of the game's own; -1: saved
    std::string bezel;     // neon | black | artwork
    std::string sync;      // display | exact
    bool windowed = false;
    bool mute = false;
    std::string ipcPath;
    long long frames = 0;      // >0: exit after N frames (automated tests)
    std::string screenshot;    // with --frames: save the last frame as BMP
    std::string inputScript;   // tests: "FRAME:control[+control],..." pressed for 8 frames
    bool audioReport = false;  // tests: print the level of the sound the game made
    int width = 0, height = 0; // with --windowed: force a window size (tests)
};

void usage() {
    std::printf(
        "zen1-engine %s\n"
        "Usage:\n"
        "  zen1-engine --game SET [options]   play a game (ROM set from ~/Zen1Arcade/Games)\n"
        "  zen1-engine --test-card [options]  cabinet test card\n"
        "  zen1-engine --list-games           games the Zen1 engine implements (JSON)\n"
        "  zen1-engine --scan                 check the Games folder (JSON)\n"
        "Options:\n"
        "  --games-dir DIR        ROM folder (default ~/Zen1Arcade/Games)\n"
        "  --zip FILE             the zip holding --game (when its name is different)\n"
        "  --rom FILE             play a console cartridge (.nes .sfc .gb .gbc .sms .gg .pce .md, or a .zip of one)\n"
        "  --display NAME|INDEX   screen to use (default: saved, else 1280x1024, else primary)\n"
        "  --scale MODE           original | integer | pixel-perfect | stretch\n"
        "  --rotate DEG           extra rotation 0 | 90 | 180 | 270\n"
        "  --bezel KIND           neon | black | artwork\n"
        "  --sync KIND            display (smoothest) | exact (original speed)\n"
        "  --mute                 no sound\n"
        "  --windowed, --size WxH window instead of fullscreen (development)\n"
        "  --ipc PATH             frontend socket\n"
        "  --frames N             quit after N frames (tests)\n"
        "  --screenshot FILE.bmp  with --frames: save the final frame\n"
        "  --input-script SPEC    tests: presses, e.g. 120:start,300:lk+right,400-600:right (p2.lk: player 2)\n"
        "  --audio-report         tests: print how loud the game's sound was\n",
        kVersion);
}

// Test input: "120:start,300:lk+right,p2.lk,400-600:right" -> presses held
// for 8 frames, or over a frame range.
struct ScriptedPress {
    long long frame, until;
    int player;
    std::string control;
};
std::vector<ScriptedPress> parseInputScript(const std::string& spec) {
    std::vector<ScriptedPress> out;
    std::stringstream ss(spec);
    std::string item;
    while (std::getline(ss, item, ',')) {
        const auto colon = item.find(':');
        if (colon == std::string::npos) continue;
        const std::string when = item.substr(0, colon);
        const long long frame = std::atoll(when.c_str());
        const auto dash = when.find('-');
        const long long until = dash == std::string::npos ? frame + 8 : std::atoll(when.substr(dash + 1).c_str());
        std::stringstream cs(item.substr(colon + 1));
        std::string c;
        while (std::getline(cs, c, '+')) {
            int player = 0;
            if (c.rfind("p2.", 0) == 0) { player = 1; c = c.substr(3); }
            out.push_back({frame, until, player, c});
        }
    }
    return out;
}
void applyInputScript(const std::vector<ScriptedPress>& script, long long frame, emu::CabinetInput& in) {
    static const char* names[6] = {"lp", "mp", "hp", "lk", "mk", "hk"};
    for (const auto& p : script) {
        if (frame < p.frame || frame >= p.until) continue;
        auto& o = in.player[static_cast<size_t>(p.player)];
        if (p.control == "up") o.up = true;
        else if (p.control == "down") o.down = true;
        else if (p.control == "left") o.left = true;
        else if (p.control == "right") o.right = true;
        else if (p.control == "start") o.start = true;
        else if (p.control == "coin") o.coin = true;
        for (size_t b = 0; b < 6; ++b)
            if (p.control == names[b]) o.button[b] = true;
    }
}

bool parseArgs(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--test-card") o.mode = Mode::TestCard;
        else if (a == "--game") { o.mode = Mode::Game; o.game = next(); }
        else if (a == "--cps1-selftest") o.mode = Mode::SelfTest;
        else if (a == "--list-games") o.mode = Mode::ListGames;
        else if (a == "--scan") o.mode = Mode::Scan;
        else if (a == "--games-dir") o.gamesDir = next();
        else if (a == "--zip") o.zip = next();
        else if (a == "--rom") { o.mode = Mode::Game; o.rom = next(); }
        else if (a == "--display") o.display = next();
        else if (a == "--scale") { o.scale = next(); if (!scaleModeFromString(o.scale)) return false; }
        else if (a == "--rotate") { o.rotate = std::atoi(next().c_str()); if (!rotationFromDegrees(o.rotate)) return false; }
        else if (a == "--bezel") o.bezel = next();
        else if (a == "--sync") o.sync = next();
        else if (a == "--mute") o.mute = true;
        else if (a == "--windowed") o.windowed = true;
        else if (a == "--ipc") o.ipcPath = next();
        else if (a == "--frames") o.frames = std::atoll(next().c_str());
        else if (a == "--screenshot") o.screenshot = next();
        else if (a == "--input-script") o.inputScript = next();
        else if (a == "--audio-report") o.audioReport = true;
        else if (a == "--size") {
            const std::string s = next();
            if (std::sscanf(s.c_str(), "%dx%d", &o.width, &o.height) != 2) return false;
        } else if (a == "--help" || a == "-h") return false;
        else return false;
    }
    return o.mode != Mode::None && (o.mode != Mode::Game || !o.game.empty() || !o.rom.empty());
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
        // An external 1280x1024 cabinet screen wins over the Deck's own panel.
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

// Audio out with gentle rate control: the queue is kept near its target by
// nudging the playback rate (at most 1.5%), so video can follow the
// display's refresh without audio drifting or crackling.
class AudioOut {
public:
    bool open(int rate) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            log::warn(std::string("audio unavailable: ") + SDL_GetError());
            return false;
        }
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, rate};
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!stream_) {
            log::warn(std::string("audio device: ") + SDL_GetError());
            return false;
        }
        bytesPerSec_ = rate * 4;
        SDL_SetAudioStreamGain(stream_, gain_);
        SDL_ResumeAudioStreamDevice(stream_);
        log::info("audio: " + std::to_string(rate) + " Hz stereo");
        return true;
    }
    void push(const std::vector<std::int16_t>& s) {
        if (!stream_ || s.empty()) return;
        const int target = bytesPerSec_ * 60 / 1000;  // 60 ms
        int queued = SDL_GetAudioStreamQueued(stream_);
        if (queued > target * 5) {  // after a stall: drop the backlog instead of lagging
            SDL_ClearAudioStream(stream_);
            queued = 0;
        }
        const double err = static_cast<double>(queued - target) / target;
        ratio_ = ratio_ * 0.9 + std::clamp(1.0 + err * 0.01, 0.985, 1.015) * 0.1;
        SDL_SetAudioStreamFrequencyRatio(stream_, static_cast<float>(ratio_));
        SDL_PutAudioStreamData(stream_, s.data(), static_cast<int>(s.size() * sizeof(std::int16_t)));
    }
    ~AudioOut() {
        if (stream_) SDL_DestroyAudioStream(stream_);
    }
    void setGain(float g) {
        gain_ = g;
        if (stream_) SDL_SetAudioStreamGain(stream_, g);
    }
    void clear() {
        if (stream_) SDL_ClearAudioStream(stream_);
    }

private:
    SDL_AudioStream* stream_ = nullptr;
    int bytesPerSec_ = 0;
    double ratio_ = 1.0;
    float gain_ = 1.0f;
};

// Per-game settings in Configurations/games/<set>.toml override the
// defaults in display.toml; command-line options override both.
struct GameDisplay {
    ScaleMode scale = ScaleMode::Original;
    int rotate = 0;
    std::string bezel = "neon";
    std::string sync = "display";
    std::optional<Rect> crop;  // learned picture area (Smart)
    std::string anchor = "auto";
    bool cropLocked = false;   // set by the user: not changed automatically
};

// Battery-backed memory (Neo Geo settings and high scores) lives in
// ~/Zen1Arcade/Saves/<set>.nv.
void loadNvram(const DataPaths& paths, const std::string& set, emu::Machine& m) {
    std::ifstream f(paths.saves() / (set + ".nv"), std::ios::binary);
    if (!f) return;
    std::vector<std::uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (!d.empty()) m.setNvram(d);
}
void saveNvram(const DataPaths& paths, const std::string& set, const emu::Machine& m) {
    const auto d = m.nvram();
    if (d.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(paths.saves(), ec);
    const auto file = paths.saves() / (set + ".nv");
    const auto tmp = file.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
        if (!f) return;
    }
    std::filesystem::rename(tmp, file, ec);
}

GameDisplay resolveDisplay(const DataPaths& paths, const std::string& set, const Options& o) {
    Settings global, game;
    global.load(paths.displayFile());
    if (!set.empty()) game.load(paths.configs() / "games" / (set + ".toml"));
    auto pick = [&](const std::string& key, const std::string& cli, const std::string& def) {
        if (!cli.empty()) return cli;
        if (game.has("display." + key)) return game.getString("display." + key);
        return global.getString("game." + key, def);
    };
    GameDisplay d;
    d.scale = scaleModeFromString(pick("scale", o.scale, "smart")).value_or(ScaleMode::Smart);
    d.crop = rectFromString(game.getString("display.crop"));
    d.anchor = pick("anchor", "", "auto");
    d.cropLocked = game.getBool("display.crop_locked", false);
    d.bezel = pick("bezel", o.bezel, "neon");
    d.sync = pick("sync", o.sync, "display");
    d.rotate = o.rotate >= 0 ? o.rotate : static_cast<int>(game.getInt("display.rotate", 0));
    return d;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        usage();
        return 2;
    }
    const DataPaths paths{defaultDataRoot()};
    const fs::path gamesDir = opt.gamesDir.empty() ? paths.games() : fs::path(opt.gamesDir);
    if (!opt.zip.empty() && !opt.game.empty()) emu::setZipOverride(opt.game, opt.zip);
    if (!opt.rom.empty()) {
        const consoles::System* sys = nullptr;
        if (const auto* s = consoles::systemForFile(opt.rom)) sys = s;
        else sys = consoles::systemInZip(opt.rom);
        if (!sys) {
            std::fprintf(stderr, "zen1-engine: %s is not a console cartridge Zen1 knows\n", opt.rom.c_str());
            return 2;
        }
        if (opt.game.empty()) opt.game = consoles::setIdFor(*sys, opt.rom);
        consoles::registerRom(opt.game, opt.rom);
    }

    if (opt.mode == Mode::ListGames) {
        std::fputs(listGamesJson().c_str(), stdout);
        return 0;
    }
    if (opt.mode == Mode::Scan) {
        std::fputs(scanJson(gamesDir).c_str(), stdout);
        return 0;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    const auto ensured = ensureDataRoot(paths.root);
    if (ensured.ok) log::openSessionLog(paths.logs(), "engine");
    log::info(std::string("zen1-engine ") + kVersion + " starting");

    IpcClient ipc;
    if (!opt.ipcPath.empty() && !ipc.connect(opt.ipcPath))
        log::warn("could not connect to frontend at " + opt.ipcPath);

    // Load the game before opening a window, so a missing ROM never flashes
    // a black screen: the frontend shows the explanation instead.
    std::unique_ptr<emu::Machine> machine;
    std::string setName;
    if (opt.mode == Mode::Game) {
        std::string err;
        machine = emu::createMachine(opt.game, gamesDir, &err);
        if (!machine) {
            log::error("cannot start " + opt.game + ": " + err);
            std::fprintf(stderr, "zen1-engine: %s\n", err.c_str());
            std::string oneLine = err;
            std::replace(oneLine.begin(), oneLine.end(), '\n', ' ');
            ipc.send("error " + oneLine);
            return 3;
        }
        setName = opt.game;
        loadNvram(paths, setName, *machine);
        log::info("game: " + opt.game);
    } else if (opt.mode == Mode::SelfTest) {
        machine = makeCps1SelfTest();
        setName = "cps1-selftest";
    }
    const GameDisplay gd = resolveDisplay(paths, machine ? setName : std::string(), opt);

    Settings displaySettings;
    displaySettings.load(paths.displayFile());
    if (opt.display.empty()) opt.display = displaySettings.getString("cabinet.sdl_display");

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
        log::error(std::string("SDL_Init failed: ") + SDL_GetError());
        ipc.send("error sdl-init");
        return 1;
    }

    const SDL_DisplayID displayId = chooseDisplay(opt.display);
    const SDL_DisplayMode* desk = SDL_GetDesktopDisplayMode(displayId);
    const int deskW = desk ? desk->w : 1280, deskH = desk ? desk->h : 1024;
    const double displayHz = desk && desk->refresh_rate > 1.0f ? desk->refresh_rate : 60.0;
    const char* dname = SDL_GetDisplayName(displayId);
    log::info("display: " + std::string(dname ? dname : "?") + " " + std::to_string(deskW) + "x" +
              std::to_string(deskH) + " @" + std::to_string(displayHz));

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
    SDL_SetRenderVSync(renderer, opt.frames > 0 ? 0 : 1);  // tests run as fast as they can

    // Source frame: the machine's native picture, or the test card.
    const emu::VideoInfo vi = machine ? machine->video()
                                      : emu::VideoInfo{kCardWidth, kCardHeight, kCardAspect, 0, 60.0};
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         vi.width, vi.height);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    std::vector<std::uint32_t> cardPixels;
    if (!machine) cardPixels.resize(static_cast<size_t>(kCardWidth * kCardHeight));
    Frame cardFrame{cardPixels.data(), kCardWidth, kCardHeight};

    const Rotation rotation =
        rotationFromDegrees(((vi.rotation + gd.rotate) % 360 + 360) % 360).value_or(Rotation::R0);
    auto presenter = std::make_unique<Presenter>(renderer, paths.artwork(), setName, gd.bezel);

    // Cabinet master volume: 0 / 50 / 100 %, shared with the frontend.
    int volume = 100;
    {
        Settings st;
        st.load(paths.settingsFile());
        volume = input::volumePreset(static_cast<int>(st.getInt("audio.volume", 100)));
    }
    auto audio = std::make_unique<AudioOut>();
    audio->setGain(input::volumeGain(volume));
    if (machine && !opt.mute) audio->open(machine->audioSampleRate());
    std::vector<std::int16_t> samples;

    auto controlsPtr = std::make_unique<Controls>();
    Controls& controls = *controlsPtr;
    controls.refresh(paths.controlsFile());
    ipc.send("ready");

    // Frame pacing: when the display refresh is close to the game's (CPS1
    // is 59.64 Hz on a 60 Hz screen) run one game frame per refresh for
    // perfectly smooth motion; otherwise follow the original speed by clock.
    const double gameHz = vi.refreshHz > 1.0 ? vi.refreshHz : 60.0;
    const bool lockToDisplay = opt.frames > 0 || (gd.sync != "exact" && std::abs(displayHz - gameHz) / gameHz < 0.015);
    const Uint64 gamePeriodNs = static_cast<Uint64>(1e9 / gameHz);
    const Uint64 minLoopNs = static_cast<Uint64>(1e9 / (lockToDisplay ? displayHz : gameHz) * 0.97);
    log::info(std::string("sync: ") + (lockToDisplay ? "one frame per display refresh" : "original speed"));

    // Smart picture: learn which part of the frame the game really uses.
    ContentTracker tracker(vi.width, vi.height);
    std::optional<Rect> crop = gd.scale == ScaleMode::Smart ? gd.crop : std::nullopt;
    if (crop) tracker.seed(*crop);
    unsigned long long gameFrames = 0;
    const auto script = parseInputScript(opt.inputScript);
    long long machineFrames = 0;
    double audioSumSq = 0;
    long long audioCount = 0;
    int audioPeak = 0;
    auto saveCrop = [&](const Rect& r) {
        const auto file = paths.configs() / "games" / (setName + ".toml");
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        Settings st;
        st.load(file);
        st.set("display.crop", rectToString(r));
        st.save(file);
        log::info("display: picture area of " + setName + " is " + rectToString(r));
    };

    CardInfo info;
    CoinStart coinStart[2];
    input::ButtonGesture gesture[2];
    HoldButton exitButton[2], restartButton[2];
    std::array<bool, 2> prevVolDown{}, prevVolUp{};
    Uint64 volumeShownUntil = 0;
    std::array<int, 4> prevDir{};
    const int ways = input::waysFromString(machine && emu::findGame(setName) ? emu::findGame(setName)->joystick : "8-way");
    Uint64 last = SDL_GetTicksNS(), loopStart = last, accumulator = 0;
    const Uint64 startedNs = last;
    double avgFrameNs = 1e9 / 60.0;
    std::string exitReason = "quit";
    bool running = true;
    unsigned long long presented = 0;

    while (running && !g_stop) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = false;
            else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
                exitReason = "keyboard";
                running = false;
            } else {
                controls.handleEvent(ev);
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

        const Uint64 now = SDL_GetTicksNS();
        controls.update();
        // Dedicated player buttons (see zen1/input/gesture.hpp):
        //   single press = coin + start
        //   double press = volume (1P down, 2P up)
        //   hold 5 s     = 1P back to the library, 2P restart the game
        const double nowS = static_cast<double>(now) / 1e9;
        const auto& lp = controls.logical();
        const auto g1 = gesture[0].update(lp[0].player, nowS);
        const auto g2 = gesture[1].update(lp[1].player, nowS);
        using GE = input::ButtonGesture::Event;
        int volumeChange = 0;  // -1 down, +1 up
        bool wantExit = false, wantRestart = false;
        if (g1.event == GE::Single) coinStart[0].trigger();
        if (g2.event == GE::Single) coinStart[1].trigger();
        if (g1.event == GE::Double) volumeChange = -1;
        if (g2.event == GE::Double) volumeChange = 1;
        if (g1.event == GE::Long) wantExit = true;
        if (g2.event == GE::Long) wantRestart = true;
        // Separately mapped system buttons, if the user set any.
        for (size_t p = 0; p < 2; ++p) {
            if (lp[p].volumeDown && !prevVolDown[p]) volumeChange = -1;
            if (lp[p].volumeUp && !prevVolUp[p]) volumeChange = 1;
            prevVolDown[p] = lp[p].volumeDown;
            prevVolUp[p] = lp[p].volumeUp;
            if (exitButton[p].update(lp[p].exitGame, nowS)) wantExit = true;
            if (restartButton[p].update(lp[p].restartGame, nowS)) wantRestart = true;
        }
        if (volumeChange) {
            const int v = volumeChange < 0 ? input::volumeDown(volume) : input::volumeUp(volume);
            log::info("volume " + std::to_string(volume) + " -> " + std::to_string(v));
            volume = v;
            audio->setGain(input::volumeGain(volume));
            Settings st;
            st.load(paths.settingsFile());
            st.set("audio.volume", static_cast<long long>(volume));
            st.save(paths.settingsFile());
            ipc.send("volume " + std::to_string(volume));
            volumeShownUntil = now + 2'000'000'000ULL;
        }
        if (wantExit) {
            exitReason = "player1-hold";
            running = false;
        }
        if (wantRestart && machine && running) {
            log::info("restart: player 2 hold");
            std::string err;
            if (opt.mode == Mode::Game) saveNvram(paths, setName, *machine);
            auto fresh = opt.mode == Mode::SelfTest ? makeCps1SelfTest() : emu::createMachine(setName, gamesDir, &err);
            if (fresh) {
                if (opt.mode == Mode::Game) loadNvram(paths, setName, *fresh);
                machine = std::move(fresh);  // the old machine and all its memory are released here
                audio->clear();
                prevDir = {};
                for (auto& c : coinStart) c = CoinStart{};
                accumulator = 0;
            } else {
                log::warn("restart failed: " + err);
            }
            ipc.send("restarted");
        }
        const double exitProgress = g1.progress;
        struct { double progress; } hold1{g1.progress}, hold2{g2.progress};

        int outW = 0, outH = 0;
        SDL_GetRenderOutputSize(renderer, &outW, &outH);
        PlanRequest req;
        req.video = {vi.width, vi.height, vi.aspect};
        req.screenWidth = outW;
        req.screenHeight = outH;
        req.mode = gd.scale;
        req.rotation = rotation;
        req.crop = crop;
        // Placement. Auto: centred, except with a scenery background the
        // picture goes to the bottom (wide games) so the scenery shows above.
        if (auto a = anchorFromString(gd.anchor)) req.anchor = *a;
        else if (gd.bezel == "scenery") req.anchor = Anchor::Bottom;
        const auto plan = computeDisplayPlan(req);

        avgFrameNs = avgFrameNs * 0.95 + static_cast<double>(now - last) * 0.05;
        last = now;

        if (machine) {
            int toRun = 1;
            if (!lockToDisplay) {
                accumulator += std::min<Uint64>(now - loopStart, gamePeriodNs * 4);
                toRun = 0;
                while (accumulator >= gamePeriodNs && toRun < 2) {
                    accumulator -= gamePeriodNs;
                    ++toRun;
                }
                if (accumulator > gamePeriodNs * 2) accumulator = 0;  // too slow: don't spiral
            }
            loopStart = now;
            for (int f = 0; f < toRun; ++f) {
                emu::CabinetInput in = toCabinetInput(controls.logical(), ways, prevDir);
                for (size_t p = 0; p < 2; ++p) {
                    coinStart[p].step(in.player[p].coin, in.player[p].start);
                    in.player[p].coin = in.player[p].coin || controls.logical()[p].coin;    // own COIN button
                    in.player[p].start = in.player[p].start || controls.logical()[p].start; // own START button
                }
                applyInputScript(script, machineFrames++, in);
                machine->setInput(in);
                machine->runFrame();
                machine->takeAudio(samples);
                if (opt.audioReport)
                    for (auto v : samples) {
                        audioSumSq += static_cast<double>(v) * v;
                        audioPeak = std::max(audioPeak, std::abs(static_cast<int>(v)));
                        ++audioCount;
                    }
                audio->push(samples);
                samples.clear();
            }
            SDL_UpdateTexture(tex, nullptr, machine->frame(), vi.width * 4);
            if (gd.scale == ScaleMode::Smart && !gd.cropLocked && opt.mode == Mode::Game) {
                tracker.feed(machine->frame(), gameFrames++);
                const bool grew = tracker.grew();
                if (auto c = tracker.crop(); c && (!crop || (grew && !(*c == *crop)))) {
                    crop = c;
                    saveCrop(*c);
                } else if (!c && crop && grew) {
                    crop.reset();  // the game uses (nearly) the whole frame after all
                    const auto file = paths.configs() / "games" / (setName + ".toml");
                    Settings st;
                    st.load(file);
                    st.remove("display.crop");
                    st.save(file);
                }
            }
        } else {
            info.panels = controls.raw();
            info.measuredHz = avgFrameNs > 0 ? 1e9 / avgFrameNs : 0.0;
            info.screenText = "SCREEN " + std::to_string(outW) + "X" + std::to_string(outH);
            info.planText = plan ? upper(toString(gd.scale)) + " " + std::to_string(plan->game.w) + "X" +
                                       std::to_string(plan->game.h)
                                 : std::string("NO PLAN");
            info.exitProgress = exitProgress;
            drawTestCard(cardFrame, info);
            ++info.frame;
            SDL_UpdateTexture(tex, nullptr, cardPixels.data(), kCardWidth * 4);
        }

        Overlay ov;
        // Show the hold bar only once it is clearly a hold, not a press.
        if (hold1.progress > 0.08) {
            ov.holdProgress = hold1.progress;
            ov.holdLabel = "KEEP HOLDING 1P: BACK TO LIBRARY";
        } else if (hold2.progress > 0.08 && machine) {
            ov.holdProgress = hold2.progress;
            ov.holdLabel = "KEEP HOLDING 2P: RESTART GAME";
        }
        if (machine && now - startedNs < 6'000'000'000ULL) {
            ov.lines.push_back("P1: " + controls.describe(0));
            ov.lines.push_back("P2: " + controls.describe(1));
            ov.lines.push_back("1P/2P: COIN+START   PRESS TWICE: VOLUME -/+   HOLD 5S: 1P LIBRARY, 2P RESTART");
        }
        if (now < volumeShownUntil) ov.volume = volume;
        presenter->draw(tex, plan ? &*plan : nullptr, rotation, ov);

        ++presented;
        if (opt.frames > 0 && static_cast<long long>(presented) >= opt.frames) {
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

        // Guard against a display that ignores vsync (and headless runs).
        const Uint64 spent = SDL_GetTicksNS() - now;
        if (opt.frames == 0 && spent < minLoopNs) SDL_DelayPrecise(minLoopNs - spent);
    }

    if (opt.audioReport)
        std::printf("audio: %lld samples, rms %.1f, peak %d\n", audioCount,
                    audioCount ? std::sqrt(audioSumSq / static_cast<double>(audioCount)) : 0.0, audioPeak);
    if (opt.mode == Mode::Game && machine) saveNvram(paths, setName, *machine);
    log::info("exit: " + exitReason);
    ipc.send("exit " + exitReason);
    machine.reset();
    // SDL objects must go before SDL_Quit.
    audio.reset();
    controlsPtr.reset();
    presenter.reset();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
