// SPDX-License-Identifier: GPL-3.0-or-later
//
// Namco Pac-Man hardware (1980) machine module: Z80 at 3.072 MHz, 36x28
// tile background, eight 16x16 sprites, PROM palette, Namco 3-voice
// waveform sound generator (WSG). Used by Pac-Man / Puck Man and, with the
// Ms. Pac-Man daughterboard decoder, Ms. Pac-Man.
//
// Hardware behaviour (memory map, tile layout, sprite placement, palette
// weights, Ms. Pac-Man decoding) follows MAME's pacman driver
// (src/mame/pacman, BSD-3-Clause, copyright-holders: Nicola Salmoria,
// Stephane Humbert, David Widel). The WSG is Zen1's own implementation.
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

namespace zen1::pacman {

// Namco WSG as used on Pac-Man: three voices, 32-sample 4-bit waveforms
// from a PROM, 20-bit frequency accumulators, 4-bit volume.
class Wsg {
public:
    void setPROM(const std::uint8_t* prom) { prom_ = prom; }
    void write(int reg, std::uint8_t v);  // 0x00-0x1f (0x5040-0x505f)
    void enable(bool on) { enabled_ = on; }
    // One output sample at the chip rate (96 kHz), signed.
    int sample();
    void reset();

private:
    const std::uint8_t* prom_ = nullptr;
    std::array<std::uint8_t, 32> regs_{};
    std::array<std::uint32_t, 3> acc_{};
    bool enabled_ = false;
};

enum class Variant { Pacman, MsPacman };

class PacmanMachine final : public emu::Machine {
public:
    PacmanMachine(const emu::GameInfo& info, Variant v, emu::RegionMap regions, std::uint8_t dsw1);
    ~PacmanMachine() override;

    emu::VideoInfo video() const override;
    int audioSampleRate() const override { return 48000; }
    void reset() override;
    void setInput(const emu::CabinetInput& in) override;
    void runFrame() override;
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override;

    // Exposed for tests.
    std::uint8_t read(std::uint16_t a);
    void write(std::uint16_t a, std::uint8_t v);
    void render();
    std::array<std::uint8_t, 0x400>& videoRam() { return vram_; }
    std::array<std::uint8_t, 0x400>& colorRam() { return cram_; }

    static constexpr int kWidth = 288;
    static constexpr int kHeight = 224;

private:
    void decodeGfx();
    void buildPalette();
    void drawSprite(int code, int color, bool fx, bool fy, int sx, int sy);
    void checkMsPacDecode(std::uint16_t a);

    emu::GameInfo info_;
    Variant variant_;
    emu::RegionMap regions_;
    std::vector<std::uint8_t> rom_;       // bank 0 (and bank 1 for Ms. Pac-Man at +0x10000)
    bool decodeBank_ = true;
    std::array<std::uint8_t, 0x400> vram_{}, cram_{};
    std::array<std::uint8_t, 0x3f0> ram_{};
    std::array<std::uint8_t, 0x10> spriteRam_{}, spriteRam2_{};
    std::array<std::uint8_t, 8> latch_{};
    std::uint8_t irqVector_ = 0;
    std::uint8_t in0_ = 0xff, in1_ = 0xff, dsw1_ = 0xc9;

    std::vector<std::uint8_t> tiles_;    // 256 tiles x 64 pixels, values 0-3
    std::vector<std::uint8_t> sprites_;  // 64 sprites x 256 pixels
    std::array<std::uint32_t, 32> rgb_{};
    std::vector<std::uint32_t> frame_;

    std::unique_ptr<::z80> cpu_;
    Wsg wsg_;
    double sampleAcc_ = 0;
    std::vector<std::int16_t> audio_;
};

// Module interface used by machines/registry.cpp
std::vector<emu::GameInfo> moduleGames();
std::vector<emu::RomFile> moduleRomFiles(const std::string& set);
std::unique_ptr<emu::Machine> moduleCreate(const std::string& set, const std::filesystem::path& gamesDir,
                                           std::string* error);

}  // namespace zen1::pacman
