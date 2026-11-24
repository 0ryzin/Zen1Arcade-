// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom CP System (CPS1) machine module for the Zen1 engine.
//
// Hardware behaviour (memory map, CPS-A/CPS-B registers, graphics decoding,
// layer and sprite rules, per-game board configuration and ROM layouts) is
// ported from MAME's CPS1 driver (src/mame/capcom/cps1.cpp, cps1_v.cpp),
// licence BSD-3-Clause, copyright-holders: Paul Leaman. See THIRD_PARTY.md.
#pragma once

#include "oki6295.hpp"
#include "zen1/emu/games.hpp"
#include "zen1/emu/machine.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct z80;

namespace zen1::cps1 {

// One entry of a CPS-B graphics bank mapper (which tile codes go to which ROM bank).
struct GfxRange {
    int type;   // GFX_* bit mask
    int start;
    int end;
    int bank;
};

enum : int { GFX_SPRITES = 1, GFX_SCROLL1 = 2, GFX_SCROLL2 = 4, GFX_SCROLL3 = 8 };

// CPS-B custom chip + B-board configuration for one game.
struct BoardConfig {
    int cpsbAddr = -1, cpsbValue = -1;
    int multFactor1 = -1, multFactor2 = -1, multLo = -1, multHi = -1;
    int layerControl = 0;
    std::array<int, 4> priority{};
    int paletteControl = 0;
    std::array<int, 5> layerEnable{};
    std::array<int, 4> bankSizes{};
    std::vector<GfxRange> mapper;
    int in2Addr = -1;  // extra buttons (kicks) on 6-button games
};

struct GameDef {
    emu::GameInfo info;
    int cpuClock = 10'000'000;
    BoardConfig board;
    std::array<std::uint8_t, 3> dips{0xff, 0xff, 0xff};  // DSWA, DSWB, DSWC defaults
    std::vector<emu::RomFile> roms;
};

const std::vector<GameDef>& gameDefs();
const GameDef* findGameDef(const std::string& set);

class Cps1Machine : public emu::Machine {
public:
    Cps1Machine(const GameDef& def, emu::RegionMap regions);
    ~Cps1Machine() override;

    emu::VideoInfo video() const override;
    int audioSampleRate() const override;
    void reset() override;
    void setInput(const emu::CabinetInput& in) override;
    void runFrame() override;
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override;

    // Memory interface (used by the CPU adapters).
    std::uint16_t read16(std::uint32_t addr);
    std::uint8_t read8(std::uint32_t addr);
    void write16(std::uint32_t addr, std::uint16_t data, std::uint16_t mask = 0xffff);
    void write8(std::uint32_t addr, std::uint8_t data);
    std::uint8_t z80Read(std::uint16_t addr);
    void z80Write(std::uint16_t addr, std::uint8_t data);
    void acknowledgeIrq() { irqPending_ = false; }
    bool irqPending() const { return irqPending_; }

    // Exposed for unit tests of the renderer.
    void renderFrame();
    std::vector<std::uint16_t>& gfxram() { return gfxram_; }
    std::array<std::uint16_t, 0x20>& cpsA() { return cpsA_; }
    std::array<std::uint16_t, 0x20>& cpsB() { return cpsB_; }
    void buildPalette();
    void latchObjects();

private:
    struct Cpu68k;
    struct Ym;

    std::uint16_t cpsBRead(int offset);
    void cpsBWrite(int offset, std::uint16_t data, std::uint16_t mask);
    void cpsAWrite(int offset, std::uint16_t data, std::uint16_t mask);
    const std::uint16_t* base(int reg, int boundary) const;
    int bankMapper(int type, int code) const;
    void decodeGfx();
    void drawTilemap(int layer, bool highOnly);
    void drawSprites();
    void runSoundUntil(double z80Cycle);
    void generateAudio(int samples);

    const GameDef& def_;
    emu::RegionMap regions_;
    std::vector<std::uint8_t>* mainRom_ = nullptr;
    std::vector<std::uint8_t>* audioRom_ = nullptr;
    std::vector<std::uint8_t>* gfxRom_ = nullptr;
    std::vector<std::uint8_t> gfxPixels_;  // 1 byte per pixel, 8 per 4-byte group

    std::vector<std::uint16_t> mainRam_ = std::vector<std::uint16_t>(0x8000);
    std::vector<std::uint16_t> gfxram_ = std::vector<std::uint16_t>(0x20000);  // 0x40000 bytes (bases wrap at 0x3ffff)
    std::array<std::uint16_t, 0x20> cpsA_{};
    std::array<std::uint16_t, 0x20> cpsB_{};
    std::vector<std::uint16_t> bufferedObj_ = std::vector<std::uint16_t>(0x400);
    std::array<std::uint32_t, 0xc00> palette_{};

    // Screen: native visible area 384x224 of a 512x262 raster.
    static constexpr int kWidth = 384, kHeight = 224, kXOff = 64, kYOff = 16;
    std::vector<std::uint16_t> pens_ = std::vector<std::uint16_t>(kWidth * kHeight);
    std::vector<std::uint8_t> prio_ = std::vector<std::uint8_t>(kWidth * kHeight);
    std::vector<std::uint32_t> frame_ = std::vector<std::uint32_t>(kWidth * kHeight);

    // Inputs (active low, as the hardware reads them).
    std::uint8_t in0_ = 0xff;
    std::uint16_t in1_ = 0xffff;
    std::uint8_t in2_ = 0xff;

    // Sound board.
    std::unique_ptr<::z80> z80_;
    std::array<std::uint8_t, 0x800> z80Ram_{};
    std::uint8_t soundLatch_ = 0, soundLatch2_ = 0;
    int audioBank_ = 0;
    std::unique_ptr<Ym> ym_;
    Oki6295 oki_;
    double okiPhase_ = 0.0;
    std::int32_t okiSample_ = 0;
    std::vector<std::int16_t> audio_;

    std::unique_ptr<Cpu68k> cpu_;
    bool irqPending_ = false;
    double z80Cycle_ = 0.0;      // target cycle count
    double ymSampleAcc_ = 0.0;   // fractional YM samples owed
};

}  // namespace zen1::cps1
