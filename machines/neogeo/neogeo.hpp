// SPDX-License-Identifier: GPL-3.0-or-later
//
// SNK Neo Geo MVS machine module: 68000 at 12 MHz, Z80 at 4 MHz, YM2610
// (FM, SSG, ADPCM-A/B through ymfm), LSPC2 video (381 hardware sprites with
// shrinking, fix layer, 2 x 4096 colour palette), uPD4990A calendar,
// backup RAM, the MVS BIOS (neogeo.zip) and cartridges.
//
// Hardware behaviour (memory map, interrupts, sprite and fix layer rules,
// Z80 banking, system latch) follows MAME's Neo Geo driver
// (src/mame/snk, BSD-3-Clause, copyright-holders: Bryan McPhail, Ernesto
// Corvi, Andrew Prime, Zsolt Vasvari). The machine glue, timing loop and
// calendar chip are Zen1's own.
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

namespace zen1::neogeo {

// NEC uPD4990A serial calendar chip (time read and the TP timing pulse).
class Upd4990a {
public:
    void write(bool din, bool clk, bool stb);
    bool dataOut(double seconds) const;
    bool tp(double seconds) const;

private:
    std::uint64_t shift_ = 0;  // 52-bit register: 48 data bits + 4 command bits
    int mode_ = 0;             // 0 hold, 1 shift, 2 time set, 3 time read
    double tpHz_ = 64.0;
    bool clk_ = false, stb_ = false;
};

class NeoGeoMachine final : public emu::Machine {
public:
    NeoGeoMachine(const emu::GameInfo& info, emu::RegionMap regions);
    ~NeoGeoMachine() override;

    emu::VideoInfo video() const override;
    int audioSampleRate() const override;
    void reset() override;
    void setInput(const emu::CabinetInput& in) override;
    void runFrame() override;
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override;
    std::vector<std::uint8_t> nvram() const override;
    void setNvram(const std::vector<std::uint8_t>& data) override;

    static constexpr int kWidth = 320;
    static constexpr int kHeight = 224;

    // Exposed for tests.
    std::uint16_t read16(std::uint32_t a);
    std::uint8_t read8(std::uint32_t a);
    void write16(std::uint32_t a, std::uint16_t v);
    void write8(std::uint32_t a, std::uint8_t v);
    std::uint8_t z80Read(std::uint16_t a);
    void z80Write(std::uint16_t a, std::uint8_t v);
    std::uint8_t z80In(std::uint16_t port);
    void z80Out(std::uint16_t port, std::uint8_t v);
    void renderLine(int line);
    std::vector<std::uint16_t>& vram() { return vram_; }
    int ipl() const;

private:
    struct Cpu68k;
    struct Ym;
    void videoWrite(int reg, std::uint16_t v);
    std::uint16_t videoRead(int reg);
    void systemLatch(int bit, bool value);
    void writePalette(int index, std::uint16_t v);
    void updateIpl();
    void soundCommand(std::uint8_t v);
    void updateNmi();
    void runZ80Until(double cycle);
    void generateAudio(int samples);
    void armTimerRelative(std::int64_t from);
    std::int64_t clock() const;

    emu::GameInfo info_;
    emu::RegionMap regions_;
    const std::vector<std::uint8_t>* prog_ = nullptr;
    const std::vector<std::uint8_t>* bios_ = nullptr;
    const std::vector<std::uint8_t>* fixCart_ = nullptr;
    const std::vector<std::uint8_t>* fixBios_ = nullptr;
    const std::vector<std::uint8_t>* zoomY_ = nullptr;
    std::vector<std::uint8_t> z80Cart_;   // M1 with the 64 KB "reload" bank base
    const std::vector<std::uint8_t>* z80Bios_ = nullptr;
    const std::vector<std::uint8_t>* adpcmA_ = nullptr;
    const std::vector<std::uint8_t>* adpcmB_ = nullptr;
    std::vector<std::uint8_t> sprites8_;  // decoded 8-bit pixels
    std::uint32_t spriteMask_ = 0;

    std::array<std::uint8_t, 0x10000> ram_{};
    std::array<std::uint8_t, 0x10000> sram_{};
    std::vector<std::uint16_t> vram_;
    std::array<std::uint16_t, 0x2000> palette_{};
    std::array<std::uint32_t, 0x4000> pens_{};  // 2 banks, normal + shadow
    std::uint32_t bankBase_ = 0;

    // LSPC
    std::uint16_t vramAddr_ = 0, vramModulo_ = 0, vramRead_ = 0;
    std::uint8_t animSpeed_ = 0, animCounter_ = 0, animFrame_ = 0;
    bool animDisabled_ = false;
    std::uint16_t timerControl_ = 0;
    std::uint32_t displayCounter_ = 0;
    std::int64_t timerAt_ = -1;  // 68000 clock of the next display-position event
    bool irqVblank_ = false, irqRaster_ = false, irqCold_ = true;
    int line_ = 0;
    std::int64_t frameStart_ = 0;

    // System latch
    bool shadow_ = false, cartVectors_ = false, cartAudio_ = false, sramUnlocked_ = false;
    int paletteBank_ = 0;

    // Inputs
    std::uint8_t p1_ = 0xff, p2_ = 0xff, coins_ = 0xff, startSel_ = 0x0f;

    // Sound
    std::unique_ptr<::z80> z80_;
    std::unique_ptr<Ym> ym_;
    std::array<std::uint32_t, 4> z80Bank_{};
    std::array<std::uint8_t, 0x800> z80Ram_{};
    std::uint8_t soundLatch_ = 0, soundReply_ = 0;
    bool latchPending_ = false, nmiEnabled_ = false, nmiLine_ = false;
    double z80Cycle_ = 0, sampleAcc_ = 0;
    std::vector<std::int16_t> audio_;

    Upd4990a rtc_;
    double seconds_ = 0;  // emulated time, for the calendar pulse

    std::vector<std::uint32_t> frame_;
    std::unique_ptr<Cpu68k> cpu_;
};

// Module interface used by machines/registry.cpp
std::vector<emu::GameInfo> moduleGames();
std::vector<emu::RomFile> moduleRomFiles(const std::string& set);
std::vector<std::string> moduleMissing(const std::string& set, const std::filesystem::path& gamesDir);
std::unique_ptr<emu::Machine> moduleCreate(const std::string& set, const std::filesystem::path& gamesDir,
                                           std::string* error);

}  // namespace zen1::neogeo
