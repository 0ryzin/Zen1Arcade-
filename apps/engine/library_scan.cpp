// SPDX-License-Identifier: GPL-3.0-or-later
#include "library_scan.hpp"

#include "zen1/emu/switch_package.hpp"
#include "consoles.hpp"

#include "zen1/emu/games.hpp"
#include "zen1/emu/known_games.hpp"
#include "zen1/emu/roms.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
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
    struct State {
        std::string status = "missing";
        std::string zip;
        std::vector<std::string> missing;
        std::string identifiedFrom;  // zip whose contents turned out to be this set
    };
    std::map<std::string, State> states;
    const auto games = emu::supportedGames();
    for (const auto& g : games) {
        known.insert(g.set);
        if (!g.parent.empty()) known.insert(g.parent);
        auto& st = states[g.set];
        if (const auto zip = emu::findSetZip(gamesDir, g.set)) {
            st.zip = zip->string();
            st.missing = emu::missingRomFiles(g.set, gamesDir);
            st.status = st.missing.empty() ? "ready" : "incomplete";
        }
    }
    // A zip whose files do not match its name's set may hold another
    // revision (or come from another ROM-set version): identify it by CRC.
    auto adopt = [&](const fs::path& zip) -> bool {
        const auto id = emu::identifyZip(zip, gamesDir);
        if (!id || states[*id].status == "ready") return false;
        auto& st = states[*id];
        st.status = "ready";
        st.zip = zip.string();
        st.missing.clear();
        st.identifiedFrom = zip.filename().string();
        return true;
    };
    for (const auto& g : games) {
        auto& st = states[g.set];
        if (st.status != "incomplete") continue;
        const fs::path zip = st.zip;
        if (const auto id = emu::identifyZip(zip, gamesDir); id && *id != g.set && states[*id].status != "ready") {
            adopt(zip);
            st = State{};  // this zip is shown as the set it really is
        }
    }
    std::set<std::string> adoptedZips;
    {
        std::error_code e2;
        for (auto it = fs::recursive_directory_iterator(gamesDir, fs::directory_options::skip_permission_denied, e2);
             !e2 && it != fs::recursive_directory_iterator(); it.increment(e2)) {
            if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
            if (!it->is_regular_file(e2) || lower(it->path().extension().string()) != ".zip") continue;
            if (known.count(lower(it->path().stem().string()))) continue;
            if (adopt(it->path())) adoptedZips.insert(it->path().string());
        }
    }
    for (const auto& g : games) {
        const auto& st = states[g.set];
        o << (first ? "" : ",") << "\n {";
        first = false;
        gameFields(o, g);
        o << ",\"supported\":true,\"status\":" << json(st.status) << ",\"zip\":" << json(st.zip)
          << ",\"identifiedFrom\":" << json(st.identifiedFrom) << ",\"missing\":[";
        for (size_t i = 0; i < st.missing.size(); ++i) o << (i ? "," : "") << json(st.missing[i]);
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
            if (consoles::systemForFile(it->path())) {
                others.push_back(it->path());
                continue;
            }
            if (ext != ".zip" && ext != ".7z" && ext != ".nsp" && ext != ".xci") continue;
            if (ext == ".zip" && known.count(lower(it->path().stem().string()))) continue;
            if (adoptedZips.count(it->path().string())) continue;
            others.push_back(it->path());
        }
        std::sort(others.begin(), others.end());
        for (const auto& p : others) {
            const std::string pext = lower(p.extension().string());
            // A console cartridge, or a zip holding one.
            const consoles::System* sys = consoles::systemForFile(p);
            if (!sys && pext == ".zip") sys = consoles::systemInZip(p);
            if (sys) {
                std::string inner = p.stem().string();
                const consoles::System* real = sys;
                if (pext == ".zip" || sys->id == "gb" || sys->id == "gbc") {
                    if (auto img = consoles::readRom(p, &real, nullptr)) {
                        if (pext == ".zip") inner = fs::path(img->fileName).stem().string();
                    }
                }
                std::set<std::string> distinct;
                for (const auto& b : real->buttonNames)
                    if (!b.empty()) distinct.insert(b);
                const int buttons = static_cast<int>(distinct.size());
                o << (first ? "" : ",") << "\n {\"set\":" << json(consoles::setIdFor(*real, p))
                  << ",\"supported\":" << (real->implemented ? "true" : "false")
                  << ",\"status\":" << json(real->implemented ? "ready" : "unsupported") << ",\"zip\":" << json(p.string())
                  << ",\"rom\":" << json(p.string()) << ",\"console\":" << json(real->id)
                  << ",\"title\":" << json(consoles::titleFromFileName(inner)) << ",\"romName\":" << json(inner)
                  << ",\"year\":\"\""
                  << ",\"manufacturer\":" << json(real->manufacturer) << ",\"system\":" << json(real->name)
                  << ",\"players\":" << real->players << ",\"buttons\":" << buttons << ",\"rotation\":0"
                  << ",\"joystick\":\"8-way\",\"format\":" << json(real->format) << ",\"core\":" << json(real->core)
                  << ",\"bios\":\"\",\"missing\":[],\"buttonNames\":[";
                for (size_t i = 0; i < real->buttonNames.size(); ++i) o << (i ? "," : "") << json(real->buttonNames[i]);
                o << "]";
                if (!real->implemented) o << ",\"reason\":" << json(real->reason);
                o << "}";
                first = false;
                continue;
            }
            if (pext == ".nsp" || pext == ".xci") {
                // Nintendo Switch package: identified from its headers and
                // name only (nothing is decrypted). Not playable in Zen1.
                const auto pkg = emu::readSwitchPackage(p);
                emu::SwitchPackage info;
                if (pkg) info = *pkg;
                else emu::parseSwitchFileName(p.stem().string(), info);
                const std::string id = info.titleId.empty() ? lower(p.stem().string()) : "switch-" + lower(info.titleId);
                o << (first ? "" : ",") << "\n {\"set\":" << json(id) << ",\"supported\":false,\"status\":"
                  << json("unsupported") << ",\"zip\":" << json(p.string())
                  << ",\"format\":" << json(info.format.empty() ? (pext == ".nsp" ? "NSP package" : "XCI cartridge image")
                                                         : info.format + (info.format == "NSP" ? " package" : " cartridge image"))
                  << ",\"title\":" << json(info.name) << ",\"system\":" << json("Nintendo Switch")
                  << ",\"titleId\":" << json(info.titleId) << ",\"packageKind\":" << json(info.kind)
                  << ",\"players\":0,\"buttons\":0"
                  << ",\"reason\":" << json(pkg ? "Zen1 cannot run Nintendo Switch games (see docs/SWITCH.md)"
                                                 : "Not a readable NSP/XCI file");
                o << "}";
                first = false;
                continue;
            }
            const bool sevenZip = pext == ".7z";
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
