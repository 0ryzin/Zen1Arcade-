// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/data_root.hpp"
#include "zen1/version.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace zen1 {
namespace {

constexpr const char* kMarkerName = ".zen1-data-version";

constexpr const char* kReadme =
    "Zen1 Arcade data folder\n"
    "=======================\n"
    "\n"
    "This folder belongs to you. Updating or reinstalling Zen1 Arcade never\n"
    "deletes, moves or overwrites anything in it. Copy the whole folder to\n"
    "back up your games, saves and settings.\n"
    "\n"
    "Games/           Put arcade ROM sets here as .zip files (do not extract).\n"
    "                 Any sub-folders are fine; games are identified by content.\n"
    "                 Then choose 'Refresh library' in Zen1 Arcade.\n"
    "BIOS/            BIOS sets that some systems need (you supply them).\n"
    "Saves/           High scores, NVRAM and EEPROM, per game.\n"
    "SaveStates/      Save states, per game.\n"
    "Configurations/  settings.toml, controls.toml, display.toml and\n"
    "                 games/<set>.toml per-game overrides. Readable text files.\n"
    "Artwork/         bezels, backgrounds, marquees, titles, snaps, flyers, logos.\n"
    "                 Put your own images in Artwork/import to pick them in SETUP.\n"
    "                 Name files after the ROM set, e.g. Artwork/bezels/sf2.png.\n"
    "Videos/          Gameplay preview videos, e.g. Videos/sf2.mp4.\n"
    "Metadata/        Library database (playtime, favourites, collections).\n"
    "ShaderCache/     Rebuilt automatically; safe to delete.\n"
    "Logs/            Logs of recent sessions.\n"
    "\n"
    "Zen1 Arcade does not include or download games, ROMs or BIOS files.\n"
    "Only add files you legally own or are authorised to use.\n";

int readMarker(const fs::path& marker) {
    std::ifstream in(marker);
    int v = 0;
    if (in >> v) return v;
    return 0;
}

bool writeNewFile(const fs::path& p, const std::string& content, std::string& err) {
    // Never overwrite: open fails if the file appeared meanwhile is acceptable,
    // we check existence first and only create.
    if (fs::exists(p)) return true;
    std::ofstream out(p, std::ios::out | std::ios::trunc);
    if (!out) {
        err = "cannot create " + p.string();
        return false;
    }
    out << content;
    return static_cast<bool>(out);
}

constexpr const char* kBiosReadme =
    "Zen1 Arcade never includes BIOS files, firmware or keys. Put your own here.\n"
    "\n"
    "Arcade BIOS sets (neogeo.zip, qsound.zip, ...): in this folder or in Games.\n"
    "\n"
    "Sega 32X (required):           Sega 32X/32X_G_BIOS.BIN, 32X_M_BIOS.BIN, 32X_S_BIOS.BIN\n"
    "PC Engine CD (required):       PC Engine CD/syscard3.pce (Super CD-ROM2 System Card 3.0)\n"
    "Nintendo 64 (optional):        Nintendo 64/pif.ntsc.rom, pif.pal.rom - without them Zen1 boots\n"
    "                               the cartridge itself\n"
    "Game Boy Advance (optional):   Game Boy Advance/gba_bios.bin - without it mGBA's own BIOS is used\n"
    "Atari 7800 (optional):         Atari 7800/7800 BIOS (U).rom, 7800 BIOS (E).rom (the Atari logo)\n"
    "SNES coprocessors (as needed): Super Famicom/dsp1.rom, dsp1b.rom, dsp2.rom, dsp3.rom, dsp4.rom,\n"
    "                               cx4.rom, st010.rom, st011.rom, st018.rom (Super FX, SA-1, S-DD1 and\n"
    "                               SPC7110 need none)\n"
    "\n"
    "A game that needs a file that is not here is shown as incomplete in the library,\n"
    "with the file it needs.\n";

} // namespace

fs::path defaultDataRoot() {
    if (const char* env = std::getenv("ZEN1_DATA_ROOT"); env && *env) return fs::path(env);
    const char* home = std::getenv("HOME");
    if (!home || !*home) return fs::path("Zen1Arcade");
    return fs::path(home) / "Zen1Arcade";
}

const std::vector<fs::path>& dataFolderLayout() {
    static const std::vector<fs::path> layout = {
        "Games",
        "BIOS",
        "BIOS/Sega 32X",
        "BIOS/PC Engine CD",
        "BIOS/Nintendo 64",
        "BIOS/Game Boy Advance",
        "BIOS/Atari 7800",
        "BIOS/Super Famicom",
        "Saves",
        "SaveStates",
        "Configurations",
        "Configurations/games",
        "Artwork",
        "Artwork/bezels",
        "Artwork/backgrounds",
        "Artwork/marquees",
        "Artwork/titles",
        "Artwork/snaps",
        "Artwork/flyers",
        "Artwork/logos",
        "Artwork/import",
        "Artwork/Scenery",
        "Videos",
        "Metadata",
        "ShaderCache",
        "Logs",
    };
    return layout;
}

EnsureResult ensureDataRoot(const fs::path& root) {
    EnsureResult r;
    r.root = root;
    std::error_code ec;

    if (fs::exists(root, ec) && !fs::is_directory(root, ec)) {
        r.error = root.string() + " exists but is not a folder";
        return r;
    }

    const fs::path marker = root / kMarkerName;
    if (fs::exists(marker, ec)) {
        r.formatVersion = readMarker(marker);
        if (r.formatVersion > kDataFormatVersion) {
            std::ostringstream msg;
            msg << "data folder uses format " << r.formatVersion
                << ", this build understands up to " << kDataFormatVersion
                << "; refusing to modify it";
            r.error = msg.str();
            return r;
        }
    }

    if (!fs::exists(root, ec)) {
        if (!fs::create_directories(root, ec)) {
            r.error = "cannot create " + root.string() + ": " + ec.message();
            return r;
        }
        r.created.push_back(root);
    }

    for (const auto& rel : dataFolderLayout()) {
        const fs::path p = root / rel;
        if (fs::exists(p, ec)) continue;
        if (!fs::create_directories(p, ec) && ec) {
            r.error = "cannot create " + p.string() + ": " + ec.message();
            return r;
        }
        r.created.push_back(p);
    }

    const fs::path biosReadme = root / "BIOS" / "README.txt";
    if (!fs::exists(biosReadme, ec)) {
        if (!writeNewFile(biosReadme, kBiosReadme, r.error)) return r;
        r.created.push_back(biosReadme);
    }

    const fs::path readme = root / "README.txt";
    if (!fs::exists(readme, ec)) {
        if (!writeNewFile(readme, kReadme, r.error)) return r;
        r.created.push_back(readme);
    }

    if (!fs::exists(marker, ec)) {
        if (!writeNewFile(marker, std::to_string(kDataFormatVersion) + "\n", r.error)) return r;
        r.created.push_back(marker);
        r.formatVersion = kDataFormatVersion;
    }

    r.ok = true;
    return r;
}

} // namespace zen1
