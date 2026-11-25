// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zen1's host for the ares emulation cores (see ares_machine.hpp). This is
// Zen1's own code: it plays the part ares's desktop-ui plays in ares, but
// for the cabinet: games are loaded through mia (ares's importer), pads are
// connected, and picture, sound and panel input go through Zen1's engine.
// Firmware that ares normally bundles is never shipped with Zen1: the
// user's own files are read from ~/Zen1Arcade/BIOS.
#include "ares_machine.hpp"

#include <ares/ares.hpp>
#include <mia/mia.hpp>

#include "pad.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>

namespace ares::Nintendo64 {
auto load(Node::System& node, string name) -> bool;
auto option(string name, string value) -> bool;
}  // namespace ares::Nintendo64
namespace ares::Atari2600 {
auto load(Node::System& node, string name) -> bool;
}
namespace ares::MegaDrive {
auto load(Node::System& node, string name) -> bool;
auto option(string name, string value) -> bool;
}  // namespace ares::MegaDrive
namespace ares::SuperFamicom {
auto load(Node::System& node, string name) -> bool;
auto option(string name, string value) -> bool;
}  // namespace ares::SuperFamicom
namespace ares::PCEngine {
auto load(Node::System& node, string name) -> bool;
auto option(string name, string value) -> bool;
}  // namespace ares::PCEngine

namespace zen1::aresx {
namespace {

namespace fs = std::filesystem;
using consoles::held;

constexpr int kRate = 48000;

// One firmware file the user supplies: the name ares uses inside the
// system, and the file names accepted in the BIOS folder (first = preferred).
struct Firmware {
    const char* aresName;
    std::vector<const char*> files;
    std::size_t size;  // expected size in bytes (0: any)
    bool optional = false;
};

struct Def {
    const char* id;
    const char* medium;      // mia medium of the game
    const char* system;      // mia system
    const char* rootName;    // name of the ares system node
    const char* pad;         // peripheral on the controller ports
    int ports;
    int canvasW, canvasH;    // the picture is scaled into this fixed frame
    double aspect;
    const char* biosFolder;  // under ~/Zen1Arcade/BIOS
    std::vector<Firmware> firmware;
    const char* cardMedium = nullptr;  // PC Engine CD: the System Card is a HuCard
    std::vector<const char*> cardFiles = {};
};

const std::vector<Def>& defs() {
    static const std::vector<Def> d = {
        {"n64", "Nintendo 64", "Nintendo 64", "Nintendo 64", "Gamepad", 4, 640, 480, 4.0 / 3.0, "Nintendo 64",
         // Optional: without them ares boots the cartridge itself (Zen1's high-level boot).
         {{"pif.ntsc.rom", {"pif.ntsc.rom", "pifdata.bin", "pif.rom"}, 1984, true},
          {"pif.pal.rom", {"pif.pal.rom", "pifdatapal.bin"}, 1984, true}}},
        {"a2600", "Atari 2600", "Atari 2600", "Atari 2600", "Gamepad", 2, 320, 240, 4.0 / 3.0, "Atari 2600", {}},
        {"32x", "Mega 32X", "Mega 32X", "Mega Drive", "Fighting Pad", 2, 320, 240, 4.0 / 3.0, "Sega 32X",
         {{"vector.rom", {"32X_G_BIOS.BIN", "vector.rom"}, 256},
          {"sh2.boot.mrom", {"32X_M_BIOS.BIN", "sh2.boot.mrom"}, 2048},
          {"sh2.boot.srom", {"32X_S_BIOS.BIN", "sh2.boot.srom"}, 1024}}},
        {"snes-ares", "Super Famicom", "Super Famicom", "Super Famicom", "Gamepad", 2, 512, 448, 4.0 / 3.0,
         "Super Famicom", {}},
        {"pcecd", "PC Engine CD", "PC Engine", "PC Engine", "Gamepad", 2, 512, 448, 4.0 / 3.0, "PC Engine CD", {},
         "PC Engine", {"syscard3.pce", "System Card 3.0.pce", "syscard3.0.pce", "[BIOS] Super CD-ROM System (Japan) (v3.0).pce",
                       "[BIOS] TurboGrafx CD Super System Card (USA) (v3.0).pce", "syscard2.pce", "syscard1.pce"}},
    };
    return d;
}

class AresMachine;
AresMachine* g_live = nullptr;

struct Binding {
    int player = 0;
    std::string control;
};

nall::string firstRegion(nall::string list) {
    auto parts = nall::split_and_strip(list, ",");
    return parts.empty() ? nall::string{"NTSC-U"} : parts.front();
}

std::string describe(const LoadResult& r, const char* what) {
    std::string why;
    switch (r.result) {
    case invalidROM: why = "it does not look like a valid dump"; break;
    case unsupportedMedia: why = "its hardware configuration is not supported yet"; break;
    case wrongMediaType: why = "it is a different kind of media"; break;
    case databaseNotFound: why = "the console database files are missing next to zen1-engine (reinstall Zen1)"; break;
    case romNotFoundInDatabase: why = "it is not in the console database"; break;
    case noFirmware: why = "the required BIOS file is missing or not the right one"; break;
    case romNotFound: why = "the file could not be read"; break;
    default: why = "an error occurred"; break;
    }
    std::string s = std::string("Could not load ") + what + ": " + why;
    if (r.info) s += std::string(" (") + r.info.data() + ")";
    return s;
}

bool fail(std::string* error, const std::string& e) {
    if (error) *error = e;
    return false;
}

std::vector<std::uint8_t> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// A file in `dir` whose name matches one of `names`, letter case ignored.
std::optional<fs::path> findFile(const fs::path& dir, const std::vector<const char*>& names) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return std::nullopt;
    auto lower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    for (const char* n : names)
        for (const auto& e : fs::directory_iterator(dir, ec))
            if (e.is_regular_file(ec) && lower(e.path().filename().string()) == lower(n)) return e.path();
    return std::nullopt;
}

class AresMachine final : public emu::Machine, public ares::Platform {
public:
    explicit AresMachine(const Def& def) : def_(def), frame_(static_cast<size_t>(def.canvasW * def.canvasH), 0u) {}
    ~AresMachine() override { shutdown(); }

    bool load(const fs::path& rom, const fs::path& saveDir, const fs::path& biosRoot, std::string* error) {
        if (g_live) g_live->shutdown();  // the ares cores are global: one machine at a time
        g_live = this;
        ares::platform = this;
        ares::Memory::FixedAllocator::get();
        std::error_code ec;
        fs::create_directories(saveDir, ec);
        const nall::string saves = saveDir.string().c_str();
        mia::setSaveLocation([saves] { return nall::string{saves, "/"}; });
        const nall::string fw = (biosRoot.string() + "/").c_str();
        mia::firmwareLocation = [fw] { return fw; };
        const fs::path biosDir = biosRoot / def_.biosFolder;
        const std::string id = def_.id;

        // Firmware first: a missing file is reported before anything starts.
        std::vector<std::pair<std::string, std::vector<std::uint8_t>>> firmware;
        for (const auto& f : def_.firmware) {
            const auto file = findFile(biosDir, f.files);
            if (!file && f.optional) continue;
            if (!file) {
                std::string list;
                for (const auto& d : def_.firmware)
                    if (!d.optional) list += std::string(list.empty() ? "" : ", ") + d.files.front();
                return fail(error, std::string("Missing BIOS: put your own ") + list + " in " + biosDir.string() +
                                       " (Zen1 never includes console firmware).");
            }
            auto data = readAll(*file);
            if (f.size && data.size() != f.size)
                return fail(error, "BIOS file " + file->string() + " is " + std::to_string(data.size()) +
                                       " bytes; a good dump is " + std::to_string(f.size) + " bytes.");
            firmware.emplace_back(f.aresName, std::move(data));
        }
        std::optional<fs::path> cardFile;
        if (def_.cardMedium) {
            cardFile = findFile(biosDir, def_.cardFiles);
            if (!cardFile)
                return fail(error, std::string("Missing BIOS: put your own System Card (syscard3.pce, the Super CD-ROM2 "
                                               "System Card 3.0) in ") +
                                       biosDir.string() + " (Zen1 never includes console firmware).");
        }

        game_ = mia::Medium::create(def_.medium);
        if (!game_) return fail(error, std::string("no importer for ") + def_.medium);
        if (auto r = game_->load(rom.string().c_str()); r != successful) return fail(error, describe(r, "the game"));
        const nall::string region = firstRegion(game_->pak->attribute("region"));
        if (cardFile) {
            card_ = mia::Medium::create(def_.cardMedium);
            if (!card_ || card_->load(cardFile->string().c_str()) != successful)
                return fail(error, "The System Card in " + cardFile->string() + " could not be read.");
        }

        system_ = mia::System::create(def_.system);
        if (!system_) return fail(error, "no system definition for this console");
        if (auto r = system_->load(); r != successful) return fail(error, describe(r, "the console"));
        // Replace the (empty) firmware placeholders with the user's files.
        for (auto& [name, data] : firmware) {
            for (auto& f : system_->pak->files())
                if (f->name() == name.c_str()) system_->pak->remove(f);
            firmwareData_.push_back(std::move(data));
            const auto& d = firmwareData_.back();
            system_->pak->append(name.c_str(), std::span<const u8>(d.data(), d.size()));
        }
        for (auto& f : system_->pak->files())  // a placeholder (not the user's file) is never used
            if (f->size() <= 1) system_->pak->remove(f);

        status_.clear();
        bool ok = false;
        const std::string r = region.data();
        const bool pal = r.find("PAL") != std::string::npos;
        if (id == "n64") {
#if defined(VULKAN)
            ares::Nintendo64::option("Enable GPU acceleration", "true");
#endif
            ares::Nintendo64::option("Recompiler", "true");
            ares::Nintendo64::option("Expansion Pak", "true");
            ok = ares::Nintendo64::load(root_, pal ? "[Nintendo] Nintendo 64 (PAL)" : "[Nintendo] Nintendo 64 (NTSC)");
        } else if (id == "a2600") {
            const char* n = r.find("SECAM") != std::string::npos ? "[Atari] Atari 2600 (SECAM)"
                            : pal                                 ? "[Atari] Atari 2600 (PAL)"
                                                                  : "[Atari] Atari 2600 (NTSC)";
            ok = ares::Atari2600::load(root_, n);
        } else if (id == "32x") {
            ares::MegaDrive::option("Recompiler", "true");
            ares::MegaDrive::option("TMSS", "false");
            const char* n = pal ? "[Sega] Mega 32X (PAL)"
                            : r.find("NTSC-J") != std::string::npos ? "[Sega] Mega 32X (NTSC-J)"
                                                                    : "[Sega] Mega 32X (NTSC-U)";
            ok = ares::MegaDrive::load(root_, n);
        } else if (id == "snes-ares") {
            // Which PPU renders: the scanline one is fast and runs nearly
            // every game; the dot-based one is for a handful of edge cases.
            ares::SuperFamicom::option("Pixel Accuracy", "false");
            ok = ares::SuperFamicom::load(root_, pal ? "[Nintendo] Super Famicom (PAL)" : "[Nintendo] Super Famicom (NTSC)");
        } else if (id == "pcecd") {
            ares::PCEngine::option("Pixel Accuracy", "true");
            ok = ares::PCEngine::load(root_, r.find("NTSC-U") != std::string::npos ? "[NEC] TurboDuo (NTSC-U)"
                                                                                   : "[NEC] PC Engine Duo (NTSC-J)");
        }
        if (!ok || !root_) return fail(error, "the emulation core could not start this system");

        for (const char* slot : {"Cartridge Slot", "PC Engine CD/Disc Tray"})
            if (auto port = root_->find<ares::Node::Port>(slot)) {
                port->allocate();
                port->connect();
            }
        for (int i = 0; i < def_.ports; ++i)
            if (auto port = root_->find<ares::Node::Port>({"Controller Port ", 1 + i})) {
                port->allocate(def_.pad);
                port->connect();
            }
        mapInputs();
        streams_ = root_->find<ares::Node::Audio::Stream>();
        for (auto& s : streams_) s->setResamplerFrequency(kRate);
        root_->power();
        loaded_ = true;
        if (id == "n64") {
            // ares starts paraLLEl-RDP (Vulkan) on the first run of the system.
            root_->run();
            // Tests only (ZEN1_N64_WITHOUT_GPU=1): run without the RDP, so a
            // cartridge that draws with the CPU still shows its picture.
            const bool noGpuOk = std::getenv("ZEN1_N64_WITHOUT_GPU") != nullptr;
            if (!noGpuOk && status_.find("Vulkan Enabled") == std::string::npos) {
                loaded_ = false;
                root_->unload();
                root_.reset();
                std::string detail = status_;
                while (!detail.empty() && detail.back() == '\n') detail.pop_back();
                for (auto& c : detail)
                    if (c == '\n') c = ';';
                return fail(error, "Nintendo 64 needs a Vulkan graphics driver, and none could be started on this "
                                   "computer (the Steam Deck has one). ares reported: " + detail);
            }
        }
        return true;
    }

    // ---- emu::Machine ------------------------------------------------------
    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = def_.canvasW;
        v.height = def_.canvasH;
        v.aspect = def_.aspect;
        v.refreshHz = refresh_;
        return v;
    }
    int audioSampleRate() const override { return kRate; }
    void reset() override {
        if (root_) root_->power(true);
    }
    void setInput(const emu::CabinetInput& in) override { in_ = in; }
    void runFrame() override {
        if (!root_) return;
        gotFrame_ = false;
        // One run() normally completes a frame; allow a few if the core
        // returns early to synchronise.
        for (int i = 0; i < 4 && !gotFrame_; ++i) root_->run();
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override {
        out.insert(out.end(), audio_.begin(), audio_.end());
        audio_.clear();
    }
    void persist() override {
        if (!loaded_ || !root_) return;
        root_->save();
        if (system_) system_->save(system_->location);
        if (game_) game_->save(game_->location);
    }
    std::vector<std::uint8_t> saveState() override {
        if (!root_) return {};
        auto s = root_->serialize();
        const auto* d = s.data();
        return std::vector<std::uint8_t>(d, d + s.size());
    }
    bool loadState(const std::vector<std::uint8_t>& data) override {
        if (!root_ || data.empty()) return false;
        serializer s(data.data(), static_cast<u32>(data.size()));
        return root_->unserialize(s);
    }

    // ---- ares::Platform ----------------------------------------------------
    void attach(ares::Node::Object node) override {
        if (auto stream = node->cast<ares::Node::Audio::Stream>()) {
            stream->setResamplerFrequency(kRate);
            if (std::find(streams_.begin(), streams_.end(), stream) == streams_.end()) streams_.push_back(stream);
        }
    }
    void detach(ares::Node::Object node) override {
        if (auto stream = node->cast<ares::Node::Audio::Stream>()) std::erase(streams_, stream);
    }
    std::shared_ptr<nall::vfs::directory> pak(ares::Node::Object node) override {
        const nall::string name = node->name();
        if (name == def_.rootName && system_) return system_->pak;
        if (name.endsWith(" Cartridge")) {
            if (card_) return card_->pak;
            if (game_) return game_->pak;
        }
        if (name.endsWith(" Disc") && game_) return game_->pak;
        return {};
    }
    void status(nall::string_view message) override { status_ += std::string(message.data(), message.size()) + "\n"; }
    void refreshRateHint(double hz) override {
        if (hz > 1) refresh_ = hz;
    }
    void video(ares::Node::Video::Screen, const u32* data, u32 pitch, u32 width, u32 height) override {
        gotFrame_ = true;
        if (!data || !width || !height) return;
        pitch >>= 2;
        const int cw = def_.canvasW, ch = def_.canvasH;
        for (int y = 0; y < ch; ++y) {
            const u32* src = data + static_cast<size_t>(y) * height / ch * pitch;
            std::uint32_t* dst = frame_.data() + static_cast<size_t>(y) * cw;
            for (int x = 0; x < cw; ++x) dst[x] = src[static_cast<size_t>(x) * width / cw] & 0xffffffu;
        }
    }
    void audio(ares::Node::Audio::Stream) override {
        if (streams_.empty()) return;
        while (true) {
            for (auto& s : streams_)
                if (!s->pending()) return;
            double mix[2] = {0, 0};
            for (auto& s : streams_) {
                double b[2] = {0, 0};
                if (s->read(b) == 1) b[1] = b[0];
                mix[0] += b[0];
                mix[1] += b[1];
            }
            for (double v : mix) audio_.push_back(static_cast<std::int16_t>(std::clamp(v, -1.0, 1.0) * 32767.0));
        }
    }
    void input(ares::Node::Input::Input node) override {
        const auto it = inputs_.find(node.get());
        if (it == inputs_.end()) return;
        const int player = it->second.player;
        const auto& p = in_.player[static_cast<size_t>(std::clamp(player, 0, 1))];
        const bool real = player >= 0 && player < 2;
        if (auto b = node->cast<ares::Node::Input::Button>()) {
            b->setValue(real && pressed(p, it->second.control));
        } else if (auto a = node->cast<ares::Node::Input::Axis>()) {
            // An analogue stick driven by the 8-way stick: full deflection.
            s64 v = 0;
            if (real && !cMode(p)) {
                if (it->second.control == "X-Axis") v = p.left ? -32767 : p.right ? 32767 : 0;
                if (it->second.control == "Y-Axis") v = p.up ? -32767 : p.down ? 32767 : 0;
            }
            a->setValue(v);
        }
    }

    void shutdown() {
        if (loaded_) {
            persist();
            root_->unload();
            root_.reset();
            streams_.clear();
            game_.reset();
            card_.reset();
            system_.reset();
            loaded_ = false;
        }
        if (g_live == this) {
            g_live = nullptr;
            ares::platform = nullptr;
        }
    }

private:
    // Every input node of each controller, by player and control name.
    void mapInputs() {
        inputs_.clear();
        for (auto& node : root_->find<ares::Node::Input::Input>()) {
            int player = 0;
            for (auto p = node->parent().lock(); p; p = p->parent().lock()) {
                const std::string n = p->name().data();
                if (n.rfind("Controller Port ", 0) == 0) {
                    player = std::atoi(n.c_str() + 16) - 1;
                    break;
                }
            }
            inputs_[node.get()] = {player, node->name().data()};
        }
    }

    // N64: holding HP turns the stick into the C buttons.
    bool cMode(const emu::PlayerInput& p) const { return std::string(def_.id) == "n64" && held(p, consoles::HP); }

    // Panel -> pad, by the names ares gives the controls.
    bool pressed(const emu::PlayerInput& p, const std::string& c) const {
        const std::string id = def_.id;
        const bool stick = !cMode(p);
        if (c == "Up") return p.up && stick;
        if (c == "Down") return p.down && stick;
        if (c == "Left") return p.left && stick;
        if (c == "Right") return p.right && stick;
        if (c == "Start" || c == "Run") return p.start;
        if (id == "a2600") {
            if (c == "Reset") return p.start;  // console switch: starts most games
            if (c == "Fire") return consoles::first(p) || consoles::second(p);
            if (c == "Select") return consoles::third(p);
            return false;
        }
        if (id == "pcecd") {  // II I Select, as the HuCard PC Engine
            if (c == "II") return consoles::first(p);
            if (c == "I") return consoles::second(p);
            if (c == "Select") return consoles::third(p);
            return false;
        }
        if (id == "n64") {
            // Bottom row B A Z, top row L R, and HP + stick = C buttons.
            if (c == "B") return held(p, consoles::LK);
            if (c == "A") return held(p, consoles::MK);
            if (c == "Z") return held(p, consoles::HK);
            if (c == "L") return held(p, consoles::LP);
            if (c == "R") return held(p, consoles::MP);
            if (c == "C-Up") return cMode(p) && p.up;
            if (c == "C-Down") return cMode(p) && p.down;
            if (c == "C-Left") return cMode(p) && p.left;
            if (c == "C-Right") return cMode(p) && p.right;
            return false;
        }
        if (id == "32x") {  // X Y Z / A B C
            if (c == "A") return held(p, consoles::LK);
            if (c == "B") return held(p, consoles::MK);
            if (c == "C") return held(p, consoles::HK);
            if (c == "X") return held(p, consoles::LP);
            if (c == "Y") return held(p, consoles::MP);
            if (c == "Z") return held(p, consoles::HP);
            return false;
        }
        // SNES: Y X L / B A R
        if (c == "Y") return held(p, consoles::LP);
        if (c == "X") return held(p, consoles::MP);
        if (c == "L") return held(p, consoles::HP);
        if (c == "B") return held(p, consoles::LK);
        if (c == "A") return held(p, consoles::MK);
        if (c == "R") return held(p, consoles::HK);
        return false;
    }

    const Def& def_;
    ares::Node::System root_;
    std::shared_ptr<mia::Pak> game_, card_, system_;
    std::vector<std::vector<std::uint8_t>> firmwareData_;
    std::vector<ares::Node::Audio::Stream> streams_;
    std::map<const void*, Binding> inputs_;
    emu::CabinetInput in_;
    std::vector<std::uint32_t> frame_;
    std::vector<std::int16_t> audio_;
    std::string status_;
    double refresh_ = 60.0;
    bool gotFrame_ = false;
    bool loaded_ = false;
};

}  // namespace

std::unique_ptr<emu::Machine> create(const std::string& system, const std::filesystem::path& rom,
                                     const std::filesystem::path& saveDir, const std::filesystem::path& biosRoot,
                                     std::string* error) {
    for (const auto& d : defs()) {
        if (system != d.id) continue;
        auto m = std::make_unique<AresMachine>(d);
        if (!m->load(rom, saveDir, biosRoot, error)) return nullptr;
        return m;
    }
    if (error) *error = "no ares system called " + system;
    return nullptr;
}

}  // namespace zen1::aresx
