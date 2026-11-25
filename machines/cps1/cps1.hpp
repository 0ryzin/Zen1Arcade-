// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom CP System (CPS1) machine module for the Zen1 engine.
//
// Hardware behaviour (memory map, CPS-A/CPS-B registers, graphics decoding,
// layer and sprite rules, per-game board configuration and ROM layouts) is
// ported from MAME's CPS1 driver (src/mame/capcom/cps1.cpp, cps1_v.cpp),
// licence BSD-3-Clause, copyright-holders: Paul Leaman. See THIRD_PARTY.md.
//
// The same class runs Capcom CPS-2 (cps2.cpp): CPS-A/CPS-B tile layers are
// shared; CPS-2 adds encrypted program fetches, its own sprite chip with
// priorities, raster interrupts, QSound and a serial EEPROM. Ported from
// MAME's cps2.cpp (BSD-3-Clause, Paul Leaman, Andreas Naive, Nicola Salmoria).
#pragma once

#include "oki6295.hpp"
#include "qsound.hpp"
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
    std::uint32_t gfxRegionSize = 0;                      // 0: computed from the ROM list
    std::vector<emu::RomFile> roms;
    bool cps2 = false;                                    // Capcom CPS-2 board
    bool cps2Dead = false;                                // "dead" board map (object registers at FFFFF0)
    std::uint32_t qsoundRegionSize = 0;
};

// 93C46 serial EEPROM in 16-bit mode (CPS-2 settings and high scores).
class Eeprom93c46 {
public:
    void write(bool cs, bool clk, bool di);
    bool dataOut() const { return out_; }
    std::array<std::uint16_t, 64> data;
    Eeprom93c46() { data.fill(0xffff); }

private:
    bool cs_ = false, clk_ = false, out_ = true, writeEnable_ = false;
    int bits_ = 0;
    std::uint32_t shift_ = 0;
    int readWord_ = -1, readBit_ = 0;
    void command();
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
    std::vector<std::uint8_t> nvram() const override;
    void setNvram(const std::vector<std::uint8_t>& d) override;
    void setQsoundDspRom(const std::vector<std::uint8_t>& rom) { qsound_.setDspRom(rom); }
    bool isCps2() const { return cps2_; }

    // Memory interface (used by the CPU adapters).
    std::uint16_t read16(std::uint32_t addr);
    std::uint8_t read8(std::uint32_t addr);
    void write16(std::uint32_t addr, std::uint16_t data, std::uint16_t mask = 0xffff);
    void write8(std::uint32_t addr, std::uint8_t data);
    std::uint8_t z80Read(std::uint16_t addr);
    void z80Write(std::uint16_t addr, std::uint8_t data);
    void acknowledgeIrq() { irqPending_ = false; ipl_ = 0; }
    bool programFetch(std::uint32_t addr) const;
    bool irqPending() const { return irqPending_; }

    // Exposed for unit tests of the renderer.
    void renderFrame();
    void renderLines(int y0, int y1);  // visible rows [y0, y1)
    std::array<std::uint16_t, 8>& cps2Output() { return output_; }
    std::vector<std::uint16_t>& objram(int bank) { return objram_[static_cast<size_t>(bank & 1)]; }
    void latchCps2Objects();
    const std::vector<std::uint16_t>& decryptedOpcodes() const { return decrypted_; }
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
    void drawTilemap(int layer, bool highOnly, int y0 = 0, int y1 = kHeight, std::uint8_t priBit = 0);
    void drawSprites(int y0 = 0, int y1 = kHeight);
    void drawCps2Sprites(int y0, int y1, const std::array<std::uint32_t, 8>& primasks);
    void renderCps2Lines(int y0, int y1);
    void runFrameCps2();
    std::uint16_t read16Cps2(std::uint32_t addr);
    void write16Cps2(std::uint32_t addr, std::uint16_t data, std::uint16_t mask);
    std::uint8_t z80ReadCps2(std::uint16_t addr);
    void z80WriteCps2(std::uint16_t addr, std::uint8_t data);
    void decryptProgram();
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

    // CPS-2
    bool cps2_ = false;
    std::vector<std::uint16_t> decrypted_;     // opcode view of the program ROM
    std::array<std::uint16_t, 8> output_{};    // object output registers (400000)
    std::array<std::vector<std::uint16_t>, 2> objram_{std::vector<std::uint16_t>(0x1000), std::vector<std::uint16_t>(0x1000)};
    int objBank_ = 0;
    std::vector<std::uint16_t> bufferedObj2_ = std::vector<std::uint16_t>(0x1000);
    std::uint16_t priCtrl_ = 0;
    std::array<std::vector<std::uint8_t>, 2> qram_{std::vector<std::uint8_t>(0x1000), std::vector<std::uint8_t>(0x1000)};
    std::vector<std::uint16_t> extraRam_ = std::vector<std::uint16_t>(0x2001);
    QSound qsound_;
    double qsAcc_ = 0.0;
    double z80IrqAcc_ = 0.0;
    bool z80Held_ = false;
    Eeprom93c46 eeprom_;
    std::uint16_t cps2In0_ = 0xffff, cps2In1_ = 0xffff, cps2In2_ = 0xffff;
    int rasterCounter_[3] = {0, 0, 0}, rasterReload_[3] = {0, 0, 0};
    int renderedRows_ = 0;
    int ipl_ = 0;  // CPS-2 interrupt lines: 2 = vblank (IPL1), 4 = raster (IPL2)

    std::unique_ptr<Cpu68k> cpu_;
    bool irqPending_ = false;
    double z80Cycle_ = 0.0;      // target cycle count
    double ymSampleAcc_ = 0.0;   // fractional YM samples owed
};

}  // namespace zen1::cps1
