// SPDX-License-Identifier: GPL-3.0-or-later
//
// Systems run on the ares emulation cores (ISC licence), compiled into
// Zen1's engine: Nintendo 64, Atari 2600, Sega 32X, Mega CD, and the SNES
// for cartridges with enhancement chips. Zen1 is the host: it loads the
// game through ares's own importer (mia), connects the pads, and passes
// picture, sound and the cabinet panel between ares and the engine.
#pragma once

#include "zen1/emu/machine.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace zen1::aresx {

// system: "n64", "a2600", "32x", "pcecd", "snes-ares".
// saveDir: where battery saves go; biosRoot: ~/Zen1Arcade/BIOS, where the
// user's own firmware is looked for (in a folder per system).
__attribute__((visibility("default"))) std::unique_ptr<emu::Machine> create(const std::string& system, const std::filesystem::path& rom,
                                     const std::filesystem::path& saveDir, const std::filesystem::path& biosRoot,
                                     std::string* error);

}  // namespace zen1::aresx
