// SPDX-License-Identifier: GPL-3.0-or-later
#include "consoles.hpp"

#include "cores.hpp"
#include "ares_machine.hpp"
#include "zen1/emu/zip.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <initializer_list>
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
    add({"md", "Sega Mega Drive / Genesis", "Sega", {".md", ".gen", ".smd"}, 2, {"X", "Y", "Z", "A", "B", "C"},
         "Mega Drive cartridge (.md/.gen/.bin)", "clownmdemu (Clownacy)", true, ""});
    add({"pce", "PC Engine / TurboGrafx-16", "NEC / Hudson", {".pce"}, 2, {"II", "I", "Select", "II", "I", "Select"},
         "HuCard (.pce)", "Geargrafx (Ignacio Sanchez)", true, ""});
    add({"gba", "Nintendo Game Boy Advance", "Nintendo", {".gba"}, 1, {"L", "R", "Select", "B", "A", "Select"},
         "Game Boy Advance cartridge (.gba)", "mGBA (Vicki Pfau and contributors)", true, ""});
    // On the ares cores (ISC licence), compiled into Zen1's engine.
    {
        System a{"a2600", "Atari 2600", "Atari", {".a26"}, 2, {"Fire", "Fire", "Select", "Fire", "Fire", "Select"},
                 "Atari 2600 cartridge (.a26)", "ares (Near and the ares team)", true, ""};
        a.ares = "a2600";
        add(a);
    }
    {
        System s{"32x", "Sega 32X", "Sega", {".32x"}, 2, {"X", "Y", "Z", "A", "B", "C"}, "32X cartridge (.32x)",
                 "ares (Near and the ares team)", true, ""};
        s.ares = "32x";
        s.biosFolder = "Sega 32X";
        s.biosFiles = {"32X_G_BIOS.BIN|vector.rom", "32X_M_BIOS.BIN|sh2.boot.mrom", "32X_S_BIOS.BIN|sh2.boot.srom"};
        add(s);
    }
    {
        System n{"n64", "Nintendo 64", "Nintendo", {".n64", ".z64", ".v64"}, 4, {"L", "R", "C", "B", "A", "Z"},
                 "Nintendo 64 cartridge (.z64/.n64/.v64)", "ares with paraLLEl-RDP (Vulkan)", true, ""};
        n.ares = "n64";
        n.biosFolder = "Nintendo 64";
        n.biosFiles = {"pif.ntsc.rom|pifdata.bin|pif.rom"};
        add(n);
    }
    {
        System c{"pcecd", "PC Engine CD / TurboGrafx-CD", "NEC / Hudson", {}, 2, {"II", "I", "Select", "II", "I", "Select"},
                 "CD image (.cue/.chd) in a PC Engine CD folder", "ares (Near and the ares team)", true, ""};
        c.ares = "pcecd";
        c.biosFolder = "PC Engine CD";
        c.biosFiles = {"syscard3.pce|System Card 3.0.pce|syscard3.0.pce|syscard2.pce|syscard1.pce"};
        add(c);
    }
    // Recognised so the library can explain them; not playable in Zen1 yet.
    add({"a7800", "Atari 7800", "Atari", {".a78"}, 2, {"1", "2", "Select", "1", "2", "Select"},
         "Atari 7800 cartridge (.a78)", "ProSystem (Greg Stanton; libretro maintainers)", true, ""});
    return v;
}

fs::path& savesDir() {
    static fs::path p;
    return p;
}
fs::path& biosDir() {
    static fs::path p;
    return p;
}

bool folderHas(const fs::path& file, std::initializer_list<const char*> words) {
    for (fs::path p = file.parent_path(); !p.empty() && p != p.parent_path(); p = p.parent_path()) {
        const std::string n = lower(p.filename().string());
        for (const char* w : words)
            if (n.find(w) != std::string::npos) return true;
    }
    return false;
}

// SNES cartridges with an enhancement chip (Super FX, SA-1, DSP, Cx4,
// S-DD1, SPC7110, ST01x, OBC1, S-RTC) go to ares; LakeSnes runs the rest.
bool snesHasCoprocessor(const std::vector<std::uint8_t>& d) {
    const size_t off = d.size() % 1024 == 512 ? 512 : 0;
    int best = -1;
    std::uint8_t type = 0;
    for (size_t h : {0x7fc0u, 0xffc0u, 0x40ffc0u}) {
        if (off + h + 0x20 > d.size()) continue;
        const auto* p = d.data() + off + h;
        const unsigned comp = p[0x1c] | p[0x1d] << 8, sum = p[0x1e] | p[0x1f] << 8;
        int score = (comp ^ sum) == 0xffff ? 4 : 0;
        const std::uint8_t mode = p[0x15] & 0xef;
        if ((h == 0x7fc0 && (mode == 0x20 || mode == 0x22 || mode == 0x23 || mode == 0x2a)) ||
            (h != 0x7fc0 && (mode == 0x21 || mode == 0x25 || mode == 0x2a || mode == 0x35 || mode == 0x31)))
            score += 2;
        if (score > best) {
            best = score;
            type = p[0x16];
        }
    }
    return best >= 0 && (type & 0x0f) >= 3 && (type & 0x0f) <= 6;
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
    if (ext == ".cue" || ext == ".chd")
        return folderHas(file, {"pc engine", "pc-engine", "pcengine", "pce", "turbografx", "tg16", "tg-16", "tgcd"})
                   ? systemById("pcecd")
                   : nullptr;
    if (ext == ".bin") return megaDriveFolder(file) && !folderHas(file, {" cd", "-cd", "mcd", "segacd"}) ? systemById("md") : nullptr;
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
        if (sys && sys->id == "pcecd") {  // a disc: ares reads it from the file itself
            img.fileName = file.filename().string();
            if (system) *system = sys;
            return img;
        }
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

void setDataDirs(const fs::path& saves, const fs::path& bios) {
    savesDir() = saves;
    biosDir() = bios;
}

std::vector<std::string> missingBios(const System& s) {
    std::vector<std::string> missing;
    const fs::path dir = biosDir() / s.biosFolder;
    std::error_code ec;
    for (const auto& alternatives : s.biosFiles) {
        std::vector<std::string> names;
        for (size_t a = 0, b; a <= alternatives.size(); a = b + 1) {
            b = alternatives.find('|', a);
            if (b == std::string::npos) b = alternatives.size();
            names.push_back(lower(alternatives.substr(a, b - a)));
        }
        bool found = false;
        if (fs::is_directory(dir, ec))
            for (const auto& e : fs::directory_iterator(dir, ec))
                if (std::find(names.begin(), names.end(), lower(e.path().filename().string())) != names.end()) found = true;
        if (!found) missing.push_back((dir / alternatives.substr(0, alternatives.find('|'))).string());
    }
    return missing;
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
    const bool snesChip = sys->id == "snes" && snesHasCoprocessor(img->data);
    if (!sys->ares.empty() || snesChip) {
        if (const auto m = missingBios(*sys); !m.empty()) {
            std::string list;
            for (const auto& f : m) list += (list.empty() ? "" : ", ") + f;
            if (error) *error = "Missing BIOS (Zen1 never includes console firmware): put your own " + list;
            return nullptr;
        }
        // ares reads the game from a file: a cartridge from a .zip is written out first.
        fs::path romFile = *file;
        std::error_code ec;
        if (lower(file->extension().string()) == ".zip") {
            const fs::path tmp = savesDir() / ".unpacked";
            fs::create_directories(tmp, ec);
            romFile = tmp / img->fileName;
            std::ofstream o(romFile, std::ios::binary | std::ios::trunc);
            o.write(reinterpret_cast<const char*>(d), static_cast<std::streamsize>(n));
        }
        const fs::path saves = savesDir() / set;
        return aresx::create(snesChip ? "snes-ares" : sys->ares, romFile, saves, biosDir(), error);
    }
    if (sys->id == "nes") return makeNes(d, n, error);
    if (sys->id == "snes") return makeSnes(d, n, error);
    if (sys->id == "gb") return makeGameBoy(d, n, Variant::GameBoy, error);
    if (sys->id == "gbc") return makeGameBoy(d, n, Variant::GameBoyColor, error);
    if (sys->id == "sms") return makeSega8(d, n, Variant::MasterSystem, error);
    if (sys->id == "gg") return makeSega8(d, n, Variant::GameGear, error);
    if (sys->id == "sg1000") return makeSega8(d, n, Variant::Sg1000, error);
    if (sys->id == "pce") return makePcEngine(d, n, error);
    if (sys->id == "md") return makeMegaDrive(d, n, error);
    if (sys->id == "a7800") return makeAtari7800(d, n, (biosDir() / "Atari 7800").string(), error);
    if (sys->id == "gba") {
        GbaFiles f;
        std::error_code ec;
        for (const char* name : {"gba_bios.bin", "GBA.BIOS", "gba.bin"})
            if (fs::is_regular_file(biosDir() / "Game Boy Advance" / name, ec)) {
                f.bios = (biosDir() / "Game Boy Advance" / name).string();
                break;
            }
        if (!savesDir().empty()) {
            fs::create_directories(savesDir(), ec);
            f.save = (savesDir() / (set + ".sav")).string();
        }
        return makeGba(d, n, f, error);
    }
    if (error) *error = sys->name + " is not implemented";
    return nullptr;
}

}  // namespace zen1::consoles
