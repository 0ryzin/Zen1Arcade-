// SPDX-License-Identifier: GPL-3.0-or-later
//
// Namco Galaxian hardware machine module (see galaxian.cpp).
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

namespace zen1::galaxian {

// Zen1's approximation of the Galaxian analogue sound board.
class Sound {
public:
    void reset();
    void latch(int bit, bool on);   // 0x6800-0x6807: FS1-3, HIT, -, FIRE, VOL1, VOL2
    void lfo(int bit, bool on);     // 0x6004-0x6007: background LFO DAC
    void pitch(std::uint8_t p) { pitch_ = p; }
    double sample();                // one 48 kHz sample, about -1..1

private:
    bool fs_[3] = {false, false, false};
    bool hit_ = false, fire_ = false, vol1_ = false, vol2_ = false;
    int lfoDac_ = 0;
    std::uint8_t pitch_ = 0xff;
    double tonePh_ = 0, lfoPh_ = 0, fsPh_[3] = {0, 0, 0}, firePh_ = 0;
    double hitAmp_ = 0, fireT_ = -1.0, noiseLp_ = 0;
    unsigned lfsr_ = 1;
    int noiseCount_ = 0;
};

class GalaxianMachine final : public emu::Machine {
public:
    GalaxianMachine(const emu::GameInfo& info, emu::RegionMap regions, std::array<std::uint8_t, 3> ports);
    ~GalaxianMachine() override;

    emu::VideoInfo video() const override;
    int audioSampleRate() const override { return 48000; }
    void reset() override;
    void setInput(const emu::CabinetInput& in) override;
    void runFrame() override;
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override;

    std::uint8_t read(std::uint16_t a);
    void write(std::uint16_t a, std::uint8_t v);
    void render();

    static constexpr int kWidth = 256;
    static constexpr int kHeight = 224;

private:
    void decodeGfx();
    void buildPalette();

    emu::GameInfo info_;
    emu::RegionMap regions_;
    std::array<std::uint8_t, 3> ports_;
    std::vector<std::uint8_t> rom_;
    std::array<std::uint8_t, 0x400> ram_{}, vram_{};
    std::array<std::uint8_t, 0x100> obj_{};
    std::uint8_t in0_ = 0, in1_ = 0;
    bool irqEnabled_ = false, starsEnabled_ = false;
    std::uint32_t starOrigin_ = 0;
    std::vector<std::uint8_t> tiles_, sprites_, stars_;
    std::array<std::uint32_t, 32> rgb_{};
    std::array<std::uint32_t, 64> starRgb_{};
    std::vector<std::uint32_t> frame_;
    std::unique_ptr<::z80> cpu_;
    Sound sound_;
    double sampleAcc_ = 0;
    std::vector<std::int16_t> audio_;
};

std::vector<emu::GameInfo> moduleGames();
std::vector<emu::RomFile> moduleRomFiles(const std::string& set);
std::unique_ptr<emu::Machine> moduleCreate(const std::string& set, const std::filesystem::path& gamesDir,
                                           std::string* error);

}  // namespace zen1::galaxian
