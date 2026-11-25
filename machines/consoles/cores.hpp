// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one entry point of each console core. Each core is linked into one
// object whose internal symbols are local (see third_party/CMakeLists.txt),
// so cores that share class names can live in one program: only these
// functions are visible outside it.
#pragma once

#include "zen1/emu/machine.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#define ZEN1_CORE_API __attribute__((visibility("default")))

namespace zen1::consoles {

enum class Variant { Default, GameBoy, GameBoyColor, MasterSystem, GameGear, Sg1000 };

using MachinePtr = std::unique_ptr<emu::Machine>;
ZEN1_CORE_API MachinePtr makeGameBoy(const std::uint8_t* rom, std::size_t size, Variant v, std::string* error);
ZEN1_CORE_API MachinePtr makeSega8(const std::uint8_t* rom, std::size_t size, Variant v, std::string* error);
ZEN1_CORE_API MachinePtr makePcEngine(const std::uint8_t* rom, std::size_t size, std::string* error);
ZEN1_CORE_API MachinePtr makeNes(const std::uint8_t* rom, std::size_t size, std::string* error);
ZEN1_CORE_API MachinePtr makeSnes(const std::uint8_t* rom, std::size_t size, std::string* error);
struct GbaFiles {
    std::string bios;  // the user's gba_bios.bin, if any (mGBA's own BIOS otherwise)
    std::string save;  // battery save file
};
ZEN1_CORE_API MachinePtr makeGba(const std::uint8_t* rom, std::size_t size, const GbaFiles& files, std::string* error);
ZEN1_CORE_API MachinePtr makeAtari7800(const std::uint8_t* rom, std::size_t size, const std::string& biosDir,
                                       std::string* error);
ZEN1_CORE_API MachinePtr makeMegaDrive(const std::uint8_t* rom, std::size_t size, std::string* error);

}  // namespace zen1::consoles
