// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom 1942 hardware machine module (see c1942.cpp).
#pragma once

#include "zen1/emu/games.hpp"
#include "zen1/emu/machine.hpp"
#include "zen1/emu/roms.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct z80;

namespace zen1::c1942 {

class C1942Machine final : public emu::Machine {
public:
    C1942Machine(const emu::GameInfo& info, emu::RegionMap regions, std::array<std::uint8_t, 2> dips);
    ~C1942Machine() override;

    emu::VideoInfo video() const override;
    int audioSampleRate() const override;
    void reset() override;
    void setInput(const emu::CabinetInput& in) override;
    void runFrame() override;
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override;

    std::uint8_t read(std::uint16_t a);
    void write(std::uint16_t a, std::uint8_t v);
    std::uint8_t soundRead(std::uint16_t a);
    void soundWrite(std::uint16_t a, std::uint8_t v);
    void renderLine(int line);

    static constexpr int kWidth = 256;
    static constexpr int kHeight = 224;

private:
    struct Ay;
    void decodeGfx();
    void buildPalette();

    emu::GameInfo info_;
    emu::RegionMap regions_;
    std::array<std::uint8_t, 2> dips_;
    std::array<std::uint8_t, 0x1000> ram_{};
    std::array<std::uint8_t, 0x800> fgram_{};
    std::array<std::uint8_t, 0x400> bgram_{};
    std::array<std::uint8_t, 0x80> spriteram_{};
    std::array<std::uint8_t, 0x800> soundRam_{};
    std::array<std::uint8_t, 2> latch_{};
    std::array<std::uint8_t, 2> players_{0xff, 0xff};
    std::uint8_t system_ = 0xff;
    std::uint16_t scroll_ = 0;
    int paletteBank_ = 0, bank_ = 0;
    bool soundHeld_ = false, flip_ = false;
    std::vector<std::uint8_t> chars_, tiles_, sprites_;
    std::array<std::uint32_t, 256> rgb_{};
    std::vector<std::uint32_t> frame_;
    std::unique_ptr<::z80> main_, sound_;
    std::array<std::unique_ptr<Ay>, 2> ay_;
    double mainTarget_ = 0, soundTarget_ = 0, ayAcc_ = 0, lp_ = 0;
    std::vector<std::int16_t> audio_;
};

std::vector<emu::GameInfo> moduleGames();
std::vector<emu::RomFile> moduleRomFiles(const std::string& set);
std::unique_ptr<emu::Machine> moduleCreate(const std::string& set, const std::filesystem::path& gamesDir,
                                           std::string* error);

}  // namespace zen1::c1942
