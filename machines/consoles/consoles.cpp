// SPDX-License-Identifier: GPL-3.0-or-later
#include "consoles.hpp"

#include "cores.hpp"
#include "zen1/emu/zip.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <mutex>

namespace fs = std::filesystem;

namespace zen1::consoles {

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

const std::vector<std::string> kTwoButton = {"B", "A", "Select", "B", "A", "Select"};

std::vector<System> makeSystems() {
    std::vector<System> v;
    auto add = [&](System s) { v.push_back(std::move(s)); };
    add({"nes", "Nintendo NES / Famicom", "Nintendo", {".nes"}, 2, kTwoButton, "NES cartridge (.nes)",
         "QuickNES (Nes_Emu, Shay Green; libretro maintainers)", true, ""});
    add({"snes", "Super Nintendo / Super Famicom", "Nintendo", {".sfc", ".smc"}, 2, {"Y", "X", "L", "B", "A", "R"},
         "SNES cartridge (.sfc/.smc)", "LakeSnes (angelo_wf)", true, ""});
    add({"gb", "Nintendo Game Boy", "Nintendo", {".gb"}, 1, kTwoButton, "Game Boy cartridge (.gb)",
         "Gearboy (Ignacio Sanchez)", true, ""});
    add({"gbc", "Nintendo Game Boy Color", "Nintendo", {".gbc", ".cgb"}, 1, kTwoButton,
         "Game Boy Color cartridge (.gbc)", "Gearboy (Ignacio Sanchez)", true, ""});
    add({"sms", "Sega Master System", "Sega", {".sms"}, 2, {"1", "2", "", "1", "2", ""}, "Master System cartridge (.sms)",
         "Gearsystem (Ignacio Sanchez)", true, ""});
    add({"gg", "Sega Game Gear", "Sega", {".gg"}, 1, {"1", "2", "", "1", "2", ""}, "Game Gear cartridge (.gg)",
         "Gearsystem (Ignacio Sanchez)", true, ""});
    add({"sg1000", "Sega SG-1000", "Sega", {".sg"}, 2, {"1", "2", "", "1", "2", ""}, "SG-1000 cartridge (.sg)",
         "Gearsystem (Ignacio Sanchez)", true, ""});
    add({"md", "Sega Mega Drive / Genesis", "Sega", {".md", ".gen"}, 2, {"X", "Y", "Z", "A", "B", "C"},
         "Mega Drive cartridge (.md/.gen/.bin)", "clownmdemu (Clownacy)", true, ""});
    add({"pce", "PC Engine / TurboGrafx-16", "NEC / Hudson", {".pce"}, 2, {"II", "I", "Select", "II", "I", "Select"},
         "HuCard (.pce)", "Geargrafx (Ignacio Sanchez)", true, ""});
    // Recognised so the library can explain them; not playable in Zen1 yet.
    add({"gba", "Nintendo Game Boy Advance", "Nintendo", {".gba"}, 1, {}, "Game Boy Advance cartridge (.gba)", "", false,
         "Game Boy Advance is not implemented yet (planned: the mGBA core, MPL-2.0)"});
    add({"n64", "Nintendo 64", "Nintendo", {".n64", ".z64", ".v64"}, 4, {}, "Nintendo 64 cartridge", "", false,
         "Nintendo 64 is not implemented: it needs a 3D renderer and a much larger core"});
    add({"a2600", "Atari 2600", "Atari", {".a26"}, 2, {}, "Atari 2600 cartridge (.a26)", "", false,
         "Atari 2600 is not implemented yet (candidate: Stella's core, GPL-2.0-or-later)"});
    add({"a7800", "Atari 7800", "Atari", {".a78"}, 2, {}, "Atari 7800 cartridge (.a78)", "", false,
         "Atari 7800 is not implemented yet (candidate: ProSystem, GPL-2.0-or-later)"});
    add({"32x", "Sega 32X", "Sega", {".32x"}, 2, {}, "32X cartridge (.32x)", "", false,
         "The 32X add-on is not emulated by Zen1's Mega Drive core"});
    return v;
}

std::mutex& romsMutex() {
    static std::mutex m;
    return m;
}
std::map<std::string, fs::path>& roms() {
    static std::map<std::string, fs::path> m;
    return m;
}

bool readFile(const fs::path& p, std::vector<std::uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

// .bin is a Mega Drive cartridge only in a folder that says so.
bool megaDriveFolder(const fs::path& file) {
    for (fs::path p = file.parent_path(); !p.empty() && p != p.parent_path(); p = p.parent_path()) {
        const std::string n = lower(p.filename().string());
        if (n.find("mega") != std::string::npos || n.find("genesis") != std::string::npos || n == "md" || n == "megadrive")
            return true;
    }
    return false;
}

const System* byExtension(const std::string& ext) {
    for (const auto& s : systems())
        for (const auto& e : s.extensions)
            if (e == ext) return &s;
    return nullptr;
}

// Game Boy cartridges say in their header whether they use colour.
const System* refineGameBoy(const System* s, const std::vector<std::uint8_t>& d) {
    if (!s || (s->id != "gb" && s->id != "gbc") || d.size() < 0x150) return s;
    return systemById((d[0x143] & 0x80) ? "gbc" : "gb");
}

}  // namespace

const std::vector<System>& systems() {
    static const std::vector<System> s = makeSystems();
    return s;
}

const System* systemById(const std::string& id) {
    for (const auto& s : systems())
        if (s.id == id) return &s;
    return nullptr;
}

const System* systemForFile(const fs::path& file) {
    const std::string ext = lower(file.extension().string());
    if (ext == ".bin") return megaDriveFolder(file) ? systemById("md") : nullptr;
    return byExtension(ext);
}

const System* systemInZip(const fs::path& zip) {
    emu::ZipArchive z;
    if (!z.open(zip)) return nullptr;
    const System* found = nullptr;
    for (const auto& e : z.entries()) {
        const fs::path inner = zip.parent_path() / fs::path(e.name).filename();
        if (const System* s = systemForFile(inner)) {
            if (found && found != s) return nullptr;  // a mixed archive is not one cartridge
            found = s;
        } else if (!e.name.empty() && e.name.back() != '/') {
            // Arcade ROM sets have many files with other names; one stray
            // text file next to a cartridge is fine.
            const std::string x = lower(fs::path(e.name).extension().string());
            if (x != ".txt" && x != ".nfo" && x != ".diz" && x != ".md5" && x != ".sha1") return nullptr;
        }
    }
    return found;
}

std::optional<RomImage> readRom(const fs::path& file, const System** system, std::string* error) {
    RomImage img;
    const System* sys = nullptr;
    if (lower(file.extension().string()) == ".zip") {
        emu::ZipArchive z;
        if (!z.open(file, error)) return std::nullopt;
        for (const auto& e : z.entries()) {
            const fs::path inner = file.parent_path() / fs::path(e.name).filename();
            if (const System* s = systemForFile(inner)) {
                auto d = z.read(e, error);
                if (!d) return std::nullopt;
                img.data = std::move(*d);
                img.fileName = fs::path(e.name).filename().string();
                sys = s;
                break;
            }
        }
        if (!sys) {
            if (error) *error = "no console cartridge in " + file.filename().string();
            return std::nullopt;
        }
    } else {
        sys = systemForFile(file);
        if (!sys || !readFile(file, img.data)) {
            if (error) *error = "cannot read " + file.string();
            return std::nullopt;
        }
        img.fileName = file.filename().string();
    }
    sys = refineGameBoy(sys, img.data);
    // Copier headers: 512 bytes in front of PC Engine dumps.
    if (sys->id == "pce" && img.data.size() % 8192 == 512) img.data.erase(img.data.begin(), img.data.begin() + 512);
    if (system) *system = sys;
    return img;
}

std::string setIdFor(const System& s, const fs::path& file) {
    std::string id = s.id + "-";
    bool dash = false;
    for (unsigned char c : file.stem().string()) {
        if (std::isalnum(c)) {
            id += static_cast<char>(std::tolower(c));
            dash = false;
        } else if (!dash) {
            id += '-';
            dash = true;
        }
    }
    while (!id.empty() && id.back() == '-') id.pop_back();
    if (id.size() > 90) id.resize(90);
    return id;
}

std::string titleFromFileName(const std::string& stem) {
    std::string t;
    int depth = 0;
    for (char c : stem) {
        if (c == '(' || c == '[') ++depth;
        else if ((c == ')' || c == ']') && depth > 0) --depth;
        else if (depth == 0) t += c == '_' ? ' ' : c;
    }
    // Collapse spaces, trim.
    std::string o;
    for (char c : t)
        if (!(c == ' ' && (o.empty() || o.back() == ' '))) o += c;
    while (!o.empty() && o.back() == ' ') o.pop_back();
    // "Legend of Zelda, The" -> "The Legend of Zelda"
    for (const char* art : {", The", ", A", ", An"}) {
        const std::string a = art;
        const auto at = o.find(a);
        if (at != std::string::npos && (at + a.size() == o.size() || o[at + a.size()] == ' ' || o[at + a.size()] == '-')) {
            o = a.substr(2) + " " + o.substr(0, at) + o.substr(at + a.size());
            break;
        }
    }
    return o.empty() ? stem : o;
}

void registerRom(const std::string& set, const fs::path& file) {
    std::lock_guard<std::mutex> l(romsMutex());
    roms()[set] = file;
}

std::optional<fs::path> registeredRom(const std::string& set) {
    std::lock_guard<std::mutex> l(romsMutex());
    const auto it = roms().find(set);
    if (it == roms().end()) return std::nullopt;
    return it->second;
}

std::unique_ptr<emu::Machine> create(const std::string& set, std::string* error) {
    const auto file = registeredRom(set);
    if (!file) {
        if (error) *error = "no cartridge file for " + set;
        return nullptr;
    }
    const System* sys = nullptr;
    auto img = readRom(*file, &sys, error);
    if (!img) return nullptr;
    if (!sys->implemented) {
        if (error) *error = sys->reason;
        return nullptr;
    }
    const auto* d = img->data.data();
    const auto n = img->data.size();
    if (sys->id == "nes") return makeNes(d, n, error);
    if (sys->id == "snes") return makeSnes(d, n, error);
    if (sys->id == "gb") return makeGameBoy(d, n, Variant::GameBoy, error);
    if (sys->id == "gbc") return makeGameBoy(d, n, Variant::GameBoyColor, error);
    if (sys->id == "sms") return makeSega8(d, n, Variant::MasterSystem, error);
    if (sys->id == "gg") return makeSega8(d, n, Variant::GameGear, error);
    if (sys->id == "sg1000") return makeSega8(d, n, Variant::Sg1000, error);
    if (sys->id == "pce") return makePcEngine(d, n, error);
    if (sys->id == "md") return makeMegaDrive(d, n, error);
    if (error) *error = sys->name + " is not implemented";
    return nullptr;
}

}  // namespace zen1::consoles
