// SPDX-License-Identifier: GPL-3.0-or-later
//
// zen1-diag: read-only hardware report for Zen1 Arcade.
// Shows what Linux reports for the arcade panels and screens, so Player 1/2
// detection and the 1280x1024 setup can be confirmed on the real Steam Deck.

#include "zen1/data_root.hpp"
#include "zen1/display/outputs.hpp"
#include "zen1/display_plan.hpp"
#include "zen1/input/assignment.hpp"
#include "zen1/input/bindings.hpp"
#include "zen1/input/devices.hpp"
#include "zen1/version.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace zen1;

namespace {

volatile std::sig_atomic_t g_stop = 0;
void onSignal(int) { g_stop = 1; }

void usage() {
    std::cout <<
        "zen1-diag " << kVersion << " - Zen1 Arcade hardware report (read-only)\n"
        "\n"
        "Usage: zen1-diag [command]\n"
        "  (no command)          Full report, also saved to ~/zen1-diagnostic.txt\n"
        "  --list-input-devices  Controllers and other input devices\n"
        "  --list-displays       Connected screens and their modes\n"
        "  --find-steam-names    Look for 'Player 1'/'Player 2' in Steam's config (read-only)\n"
        "  --watch [seconds]     Show live button and stick input (default 60 s, Ctrl+C stops)\n"
        "  --bind-players        Press a button on each panel to save Player 1/2 (writes\n"
        "                        ~/Zen1Arcade/Configurations/controls.toml)\n"
        "  --create-data-folder  Create ~/Zen1Arcade and its sub-folders if missing\n"
        "  --help                This text\n";
}

std::string joystickButtonName(unsigned code) {
    static const std::map<unsigned, const char*> names = {
        {BTN_TRIGGER, "BTN_TRIGGER"}, {BTN_THUMB, "BTN_THUMB"}, {BTN_THUMB2, "BTN_THUMB2"},
        {BTN_TOP, "BTN_TOP"}, {BTN_TOP2, "BTN_TOP2"}, {BTN_PINKIE, "BTN_PINKIE"},
        {BTN_BASE, "BTN_BASE"}, {BTN_BASE2, "BTN_BASE2"}, {BTN_BASE3, "BTN_BASE3"},
        {BTN_BASE4, "BTN_BASE4"}, {BTN_BASE5, "BTN_BASE5"}, {BTN_BASE6, "BTN_BASE6"},
        {BTN_DEAD, "BTN_DEAD"}, {BTN_SOUTH, "BTN_SOUTH"}, {BTN_EAST, "BTN_EAST"},
        {BTN_C, "BTN_C"}, {BTN_NORTH, "BTN_NORTH"}, {BTN_WEST, "BTN_WEST"}, {BTN_Z, "BTN_Z"},
        {BTN_TL, "BTN_TL"}, {BTN_TR, "BTN_TR"}, {BTN_TL2, "BTN_TL2"}, {BTN_TR2, "BTN_TR2"},
        {BTN_SELECT, "BTN_SELECT"}, {BTN_START, "BTN_START"}, {BTN_MODE, "BTN_MODE"},
        {BTN_THUMBL, "BTN_THUMBL"}, {BTN_THUMBR, "BTN_THUMBR"},
    };
    auto it = names.find(code);
    char buf[48];
    std::snprintf(buf, sizeof buf, "%s (code %u)", it != names.end() ? it->second : "KEY", code);
    return buf;
}

void reportDevices(std::ostream& out) {
    const auto devices = input::scanDevices();
    out << "== Input devices ==\n";
    if (devices.empty()) out << "  (none found in /dev/input)\n";
    int unreadable = 0;
    for (const auto& d : devices) {
        if (!d.readable) { ++unreadable; continue; }
        if (d.kind == input::DeviceKind::Other) continue;  // power buttons, sensors...
        out << "- " << (d.name.empty() ? "(no name)" : d.name) << "\n"
            << "    type:      " << input::toString(d.kind)
            << (d.virtualDevice ? "  [software/virtual device, ignored for players]" : "")
            << (d.valve ? "  [Valve hardware, ignored for players]" : "") << "\n"
            << "    usb id:    " << input::hex4(d.vendor) << ":" << input::hex4(d.product) << "\n"
            << "    serial:    " << (d.uniq.empty() ? "(none)" : d.uniq) << "\n"
            << "    node:      " << d.eventPath << "\n"
            << "    port path: " << (d.byPath.empty() ? "(none)" : d.byPath) << "\n"
            << "    phys:      " << (d.phys.empty() ? "(none)" : d.phys) << "\n"
            << "    controls:  " << d.buttonCount << " buttons, " << d.axisCount << " axes"
            << (d.hasHat ? ", hat (d-pad style stick)" : "") << "\n";
    }
    if (unreadable)
        out << "  (" << unreadable << " device(s) not readable by this user; keyboards often are not, that is normal)\n";

    const auto saved = input::loadPlayerBindings(DataPaths{defaultDataRoot()}.controlsFile());
    const auto a = input::assignPlayers(devices, saved);
    out << "\n== Player assignment (what Zen1 would do now) ==\n";
    for (int p = 0; p < 2; ++p) {
        out << "  Player " << (p + 1) << ": ";
        if (a.players[p].device) {
            const auto& d = devices[*a.players[p].device];
            out << d.name << "  <- " << input::toString(a.players[p].reason) << "\n";
        } else {
            out << "not assigned (would ask: press a button on Player " << (p + 1) << "'s panel)\n";
        }
    }
    int candidates = 0;
    for (const auto& d : devices) candidates += input::isPlayerCandidate(d) ? 1 : 0;
    out << "  Physical controllers found: " << candidates << "\n\n";
}

void reportDisplays(std::ostream& out) {
    const auto outputs = display::scanOutputs();
    out << "== Screens ==\n";
    if (outputs.empty()) out << "  (no DRM connectors found)\n";
    for (const auto& o : outputs) {
        if (o.status != "connected") continue;
        out << "- " << o.connector << (o.internal ? "  [built-in panel]" : "  [external]") << "\n";
        if (o.edid) {
            out << "    monitor:   " << (o.edid->monitorName.empty() ? "(unnamed)" : o.edid->monitorName)
                << "  (" << o.edid->manufacturer << " " << input::hex4(o.edid->productCode) << ")\n";
            if (o.edid->widthCm && o.edid->heightCm)
                out << "    size:      " << o.edid->widthCm << " x " << o.edid->heightCm << " cm\n";
        }
        if (!o.modes.empty()) {
            out << "    preferred: " << o.modes.front().width << " x " << o.modes.front().height << "\n";
            out << "    modes:     ";
            for (size_t i = 0; i < o.modes.size() && i < 8; ++i)
                out << (i ? ", " : "") << o.modes[i].width << "x" << o.modes[i].height;
            out << (o.modes.size() > 8 ? ", ..." : "") << "\n";
        }
    }
    const auto cab = display::guessCabinetOutput(outputs);
    out << "  Cabinet screen guess: " << (cab ? outputs[*cab].connector : std::string("(none)")) << "\n";

    if (cab && !outputs[*cab].modes.empty()) {
        const auto m = outputs[*cab].modes.front();
        PlanRequest r;
        r.video = {384, 224, 4.0 / 3.0};
        r.screenWidth = m.width;
        r.screenHeight = m.height;
        if (auto plan = computeDisplayPlan(r)) {
            out << "  Street Fighter II (384x224, 4:3) would be shown at " << plan->game.w << " x "
                << plan->game.h << ", offset (" << plan->game.x << ", " << plan->game.y << ")\n";
        }
    }
    out << "\n";
}

std::vector<fs::path> steamConfigDirs() {
    std::vector<fs::path> dirs;
    const char* home = std::getenv("HOME");
    if (!home) return dirs;
    const fs::path h(home);
    for (const fs::path& base : {h / ".local/share/Steam", h / ".steam/steam", h / ".steam/root"}) {
        std::error_code ec;
        if (!fs::exists(base, ec)) continue;
        dirs.push_back(base / "config");
        for (const auto& u : fs::directory_iterator(base / "userdata", ec))
            dirs.push_back(u.path() / "config");
    }
    return dirs;
}

void reportSteamNames(std::ostream& out) {
    out << "== Steam controller names (read-only search) ==\n";
    std::vector<std::string> seen;
    int hits = 0;
    for (const auto& dir : steamConfigDirs()) {
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
            const auto& p = it->path();
            if (!it->is_regular_file(ec) || p.extension() != ".vdf") continue;
            if (it->file_size(ec) > 5 * 1024 * 1024) continue;
            const std::string canon = fs::weakly_canonical(p, ec).string();
            bool dup = false;
            for (const auto& s : seen) dup = dup || s == canon;
            if (dup) continue;
            seen.push_back(canon);

            std::ifstream in(p);
            std::string line;
            int n = 0;
            while (std::getline(in, line)) {
                ++n;
                if (line.find("Player 1") == std::string::npos && line.find("Player 2") == std::string::npos)
                    continue;
                ++hits;
                if (line.size() > 200) line = line.substr(0, 200) + "...";
                out << "  " << p.string() << ":" << n << ": " << line << "\n";
                if (hits >= 40) break;
            }
        }
    }
    if (hits == 0) out << "  No 'Player 1'/'Player 2' text found in Steam's .vdf config files.\n";
    out << "\n";
}

// Opens player-candidate devices (or all joysticks/gamepads).
struct OpenDevice {
    input::DeviceInfo info;
    int fd = -1;
};

std::vector<OpenDevice> openControllers() {
    std::vector<OpenDevice> open;
    for (auto& d : input::scanDevices()) {
        if (!input::isPlayerCandidate(d)) continue;
        const int fd = ::open(d.eventPath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) open.push_back({d, fd});
    }
    return open;
}

int watchInput(int seconds) {
    auto devs = openControllers();
    if (devs.empty()) {
        std::cout << "No physical controllers found to watch.\n";
        return 1;
    }
    std::cout << "Watching " << devs.size() << " controller(s) for " << seconds
              << " s. Press buttons and move the sticks. Ctrl+C to stop.\n";
    for (size_t i = 0; i < devs.size(); ++i)
        std::cout << "  [" << i << "] " << devs[i].info.name << "  (" << devs[i].info.byPath << ")\n";

    std::vector<pollfd> pfds;
    for (auto& d : devs) pfds.push_back({d.fd, POLLIN, 0});
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!g_stop && std::chrono::steady_clock::now() < end) {
        if (::poll(pfds.data(), pfds.size(), 200) <= 0) continue;
        for (size_t i = 0; i < pfds.size(); ++i) {
            if (!(pfds[i].revents & POLLIN)) continue;
            input_event ev{};
            while (::read(pfds[i].fd, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev)) {
                if (ev.type == EV_KEY && ev.value != 2) {
                    std::cout << "  [" << i << "] " << devs[i].info.name << ": button "
                              << joystickButtonName(ev.code) << (ev.value ? " pressed" : " released") << "\n";
                } else if (ev.type == EV_ABS) {
                    std::cout << "  [" << i << "] " << devs[i].info.name << ": axis " << ev.code
                              << (ev.code == ABS_HAT0X ? " (hat X)" : ev.code == ABS_HAT0Y ? " (hat Y)"
                                  : ev.code == ABS_X ? " (X)" : ev.code == ABS_Y ? " (Y)" : "")
                              << " = " << ev.value << "\n";
                }
            }
        }
        std::cout.flush();
    }
    for (auto& d : devs) ::close(d.fd);
    return 0;
}

// Waits for any button press; returns the index of the device it came from.
int waitForButton(std::vector<OpenDevice>& devs, int timeoutSec, int exclude) {
    std::vector<pollfd> pfds;
    for (auto& d : devs) pfds.push_back({d.fd, POLLIN, 0});
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec);
    while (!g_stop && std::chrono::steady_clock::now() < end) {
        if (::poll(pfds.data(), pfds.size(), 200) <= 0) continue;
        for (size_t i = 0; i < pfds.size(); ++i) {
            if (!(pfds[i].revents & POLLIN)) continue;
            input_event ev{};
            while (::read(pfds[i].fd, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev)) {
                if (ev.type == EV_KEY && ev.value == 1 && static_cast<int>(i) != exclude)
                    return static_cast<int>(i);
            }
        }
    }
    return -1;
}

int bindPlayers() {
    auto devs = openControllers();
    if (devs.size() < 2) {
        std::cout << "Found " << devs.size() << " physical controller(s); two are needed.\n";
        for (auto& d : devs) ::close(d.fd);
        return 1;
    }
    std::array<input::DeviceIdentity, 2> ids;
    int first = -1;
    for (int p = 0; p < 2; ++p) {
        std::cout << "Press any button on PLAYER " << (p + 1) << "'s panel (30 s)...\n" << std::flush;
        const int i = waitForButton(devs, 30, first);
        if (i < 0) {
            std::cout << "No press received. Nothing saved.\n";
            for (auto& d : devs) ::close(d.fd);
            return 1;
        }
        ids[p] = input::DeviceIdentity::from(devs[i].info);
        std::cout << "  Player " << (p + 1) << " = " << devs[i].info.name << "  ("
                  << (devs[i].info.byPath.empty() ? devs[i].info.eventPath : devs[i].info.byPath) << ")\n";
        if (p == 0) first = i;
    }
    for (auto& d : devs) ::close(d.fd);

    const DataPaths paths{defaultDataRoot()};
    const auto ensured = ensureDataRoot(paths.root);
    if (!ensured.ok) {
        std::cout << "Cannot prepare " << paths.root << ": " << ensured.error << "\n";
        return 1;
    }
    std::string err;
    if (!input::savePlayerBindings(paths.controlsFile(), ids, &err)) {
        std::cout << "Saving failed: " << err << "\n";
        return 1;
    }
    std::cout << "Saved to " << paths.controlsFile() << "\n";
    return 0;
}

int createDataFolder() {
    const auto r = ensureDataRoot(defaultDataRoot());
    if (!r.ok) {
        std::cout << "Failed: " << r.error << "\n";
        return 1;
    }
    std::cout << "Data folder ready: " << r.root << "\n";
    for (const auto& c : r.created) std::cout << "  created " << c.string() << "\n";
    if (r.created.empty()) std::cout << "  (everything already existed; nothing changed)\n";
    return 0;
}

void reportHeader(std::ostream& out) {
    out << "Zen1 Arcade diagnostic, zen1-diag " << kVersion << "\n";
    std::ifstream osr("/etc/os-release");
    std::string line;
    while (std::getline(osr, line))
        if (line.rfind("PRETTY_NAME=", 0) == 0) out << "System: " << line.substr(12) << "\n";
    if (const char* s = std::getenv("XDG_SESSION_TYPE")) out << "Session: " << s << "\n";
    std::error_code ec;
    const auto root = defaultDataRoot();
    out << "Data folder: " << root.string()
        << (fs::exists(root, ec) ? " (exists)" : " (not created yet)") << "\n\n";
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    const std::string cmd = argc > 1 ? argv[1] : "";

    if (cmd == "--help" || cmd == "-h") { usage(); return 0; }
    if (cmd == "--list-input-devices") { reportDevices(std::cout); return 0; }
    if (cmd == "--list-displays") { reportDisplays(std::cout); return 0; }
    if (cmd == "--find-steam-names") { reportSteamNames(std::cout); return 0; }
    if (cmd == "--watch") return watchInput(argc > 2 ? std::max(1, std::atoi(argv[2])) : 60);
    if (cmd == "--bind-players") return bindPlayers();
    if (cmd == "--create-data-folder") return createDataFolder();
    if (!cmd.empty()) { usage(); return 2; }

    std::ostringstream report;
    reportHeader(report);
    reportDevices(report);
    reportDisplays(report);
    reportSteamNames(report);
    std::cout << report.str();

    if (const char* home = std::getenv("HOME")) {
        const fs::path file = fs::path(home) / "zen1-diagnostic.txt";
        std::ofstream f(file);
        if (f << report.str()) std::cout << "Report saved to " << file.string() << "\n";
    }
    return 0;
}
