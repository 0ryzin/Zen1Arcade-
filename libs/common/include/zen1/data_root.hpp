// SPDX-License-Identifier: GPL-3.0-or-later
//
// The permanent Zen1 Arcade data folder (default: ~/Zen1Arcade).
// Everything here survives application updates. Nothing in this module
// ever deletes or overwrites user files: it only creates what is missing.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace zen1 {

// $ZEN1_DATA_ROOT if set (used by tests), otherwise $HOME/Zen1Arcade.
std::filesystem::path defaultDataRoot();

// Sub-folders, relative to the data root, in creation order.
const std::vector<std::filesystem::path>& dataFolderLayout();

struct EnsureResult {
    bool ok = false;
    std::filesystem::path root;
    std::vector<std::filesystem::path> created;  // folders/files made this time
    int formatVersion = 0;                       // version found or written
    std::string error;                           // set when ok == false
};

// Creates the data root and any missing sub-folders, README.txt and the
// format marker. Refuses (ok=false) if the folder was written by a newer
// Zen1 data format, so an older build never touches newer data.
EnsureResult ensureDataRoot(const std::filesystem::path& root);

// Convenience paths.
struct DataPaths {
    std::filesystem::path root;
    std::filesystem::path games() const { return root / "Games"; }
    std::filesystem::path bios() const { return root / "BIOS"; }
    std::filesystem::path saves() const { return root / "Saves"; }
    std::filesystem::path saveStates() const { return root / "SaveStates"; }
    std::filesystem::path configs() const { return root / "Configurations"; }
    std::filesystem::path settingsFile() const { return configs() / "settings.toml"; }
    std::filesystem::path controlsFile() const { return configs() / "controls.toml"; }
    std::filesystem::path displayFile() const { return configs() / "display.toml"; }
    std::filesystem::path artwork() const { return root / "Artwork"; }
    std::filesystem::path videos() const { return root / "Videos"; }
    std::filesystem::path metadata() const { return root / "Metadata"; }
    std::filesystem::path shaderCache() const { return root / "ShaderCache"; }
    std::filesystem::path logs() const { return root / "Logs"; }
};

} // namespace zen1
