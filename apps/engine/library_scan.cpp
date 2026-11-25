// SPDX-License-Identifier: GPL-3.0-or-later
#include "library_scan.hpp"

#include "zen1/emu/games.hpp"
#include "zen1/emu/known_games.hpp"
#include "zen1/emu/roms.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace zen1::engine {

namespace {

std::string json(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                std::snprintf(b, sizeof b, "\\u%04x", c);
                o += b;
            } else {
                o += static_cast<char>(c);
            }
        }
    }
    return o + "\"";
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void gameFields(std::ostringstream& o, const emu::GameInfo& g) {
    o << "\"set\":" << json(g.set) << ",\"parent\":" << json(g.parent) << ",\"title\":" << json(g.title)
      << ",\"year\":" << json(g.year) << ",\"manufacturer\":" << json(g.manufacturer)
      << ",\"system\":" << json(g.system) << ",\"players\":" << g.players << ",\"buttons\":" << g.buttons
      << ",\"rotation\":" << g.rotation << ",\"joystick\":" << json(g.joystick)
      << ",\"format\":" << json(g.format) << ",\"bios\":" << json(g.bios) << ",\"buttonNames\":[";
    for (size_t i = 0; i < g.buttonNames.size(); ++i) o << (i ? "," : "") << json(g.buttonNames[i]);
    o << "]";
}

}  // namespace

std::string listGamesJson() {
    std::ostringstream o;
    o << "[";
    bool first = true;
    for (const auto& g : emu::supportedGames()) {
        o << (first ? "" : ",") << "\n {";
        gameFields(o, g);
        o << "}";
        first = false;
    }
    o << "\n]\n";
    return o.str();
}

std::string scanJson(const fs::path& gamesDir) {
    std::ostringstream o;
    o << "{\"games_dir\":" << json(gamesDir.string()) << ",\"games\":[";
    std::set<std::string> known;
    bool first = true;
    for (const auto& g : emu::supportedGames()) {
        known.insert(g.set);
        if (!g.parent.empty()) known.insert(g.parent);
        const auto zip = emu::findSetZip(gamesDir, g.set);
        std::vector<std::string> missing;
        std::string status = "missing";
        if (zip) {
            missing = emu::missingRomFiles(g.set, gamesDir);
            status = missing.empty() ? "ready" : "incomplete";
        }
        o << (first ? "" : ",") << "\n {";
        first = false;
        gameFields(o, g);
        o << ",\"supported\":true,\"status\":" << json(status) << ",\"zip\":" << json(zip ? zip->string() : "")
          << ",\"missing\":[";
        for (size_t i = 0; i < missing.size(); ++i) o << (i ? "," : "") << json(missing[i]);
        o << "]}";
    }

    // Everything else in the Games folder is shown as not supported.
    std::error_code ec;
    if (fs::is_directory(gamesDir, ec)) {
        std::vector<fs::path> others;
        for (auto it = fs::recursive_directory_iterator(gamesDir, fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (it.depth() > 4) {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file(ec)) continue;
            const std::string ext = lower(it->path().extension().string());
            if (ext != ".zip" && ext != ".7z") continue;
            if (ext == ".zip" && known.count(lower(it->path().stem().string()))) continue;
            others.push_back(it->path());
        }
        std::sort(others.begin(), others.end());
        for (const auto& p : others) {
            const bool sevenZip = lower(p.extension().string()) == ".7z";
            const std::string set = lower(p.stem().string());
            const auto* k = emu::findKnownGame(set);
            if (k && k->players == 0) continue;  // a BIOS file, not a game
            o << (first ? "" : ",") << "\n {\"set\":" << json(set) << ",\"supported\":false,\"status\":"
              << json("unsupported") << ",\"zip\":" << json(p.string())
              << ",\"format\":" << json(sevenZip ? "7z archive" : "ZIP ROM set");
            if (k) {
                o << ",\"title\":" << json(k->title) << ",\"year\":" << json(k->year)
                  << ",\"manufacturer\":" << json(k->manufacturer) << ",\"system\":" << json(k->hardware)
                  << ",\"players\":" << k->players << ",\"joystick\":" << json(k->joystick)
                  << ",\"buttons\":" << k->buttons << ",\"bios\":" << json(k->bios)
                  << ",\"reason\":" << json("Zen1 does not emulate this yet: " + k->missing);
            } else {
                o << ",\"title\":" << json(p.stem().string()) << ",\"reason\":"
                  << json(sevenZip ? "7z archives are not read yet; use the .zip ROM set"
                                   : "Not a game Zen1 recognises, or its hardware is not implemented yet");
            }
            o << "}";
            first = false;
        }
    }
    o << "\n]}\n";
    return o.str();
}

}  // namespace zen1::engine
