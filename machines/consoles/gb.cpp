// SPDX-License-Identifier: GPL-3.0-or-later
//
// Game Boy and Game Boy Color, on the Gearboy core (Ignacio Sanchez, GPL-3.0).
#include "cores.hpp"
#include "pad.hpp"

#include "gearboy.h"

#include <sstream>

// The core's logger stays quiet when this is set (meant for its MCP mode):
// Zen1 keeps the engine's output for its own log.
bool g_mcp_stdio_mode = true;

namespace zen1::consoles {
namespace {

class GameBoyMachine final : public emu::Machine {
public:
    bool load(const std::uint8_t* rom, std::size_t size, bool forceDmg, std::string* error) {  // the header decides colour unless forced
        core_.Init(GB_PIXEL_RGB565);
        core_.SetSoundSampleRate(GB_AUDIO_SAMPLE_RATE);
        core_.SetSGBEnabled(false);
        if (!core_.LoadROMFromBuffer(rom, static_cast<int>(size), forceDmg)) {
            if (error) *error = "Gearboy could not load this Game Boy cartridge (unsupported mapper or bad dump)";
            return false;
        }
        forceDmg_ = forceDmg;
        return true;
    }

    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = GAMEBOY_WIDTH;
        v.height = GAMEBOY_HEIGHT;
        v.aspect = 10.0 / 9.0;
        v.refreshHz = 59.7275;
        return v;
    }
    int audioSampleRate() const override { return GB_AUDIO_SAMPLE_RATE; }
    void reset() override { core_.ResetROMPreservingRAM(forceDmg_); }

    void setInput(const emu::CabinetInput& in) override {
        const auto& p = in.player[0];
        set(Up_Key, p.up);
        set(Down_Key, p.down);
        set(Left_Key, p.left);
        set(Right_Key, p.right);
        set(B_Key, first(p));
        set(A_Key, second(p));
        set(Select_Key, third(p));
        set(Start_Key, p.start);
    }

    void runFrame() override {
        int samples = 0;
        core_.RunToVBlank(fb_, audio_, &samples);
        for (int i = 0; i < GAMEBOY_WIDTH * GAMEBOY_HEIGHT; ++i) frame_[static_cast<size_t>(i)] = rgb565(fb_[i]);
        out_.stereo(audio_, samples / 2);
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override { out_.take(out); }

    std::vector<std::uint8_t> nvram() const override {
        auto* rule = const_cast<GearboyCore&>(core_).GetMemory()->GetCurrentRule();
        if (!rule || !const_cast<GearboyCore&>(core_).GetCartridge()->HasBattery() || rule->GetRamSize() == 0) return {};
        std::ostringstream s;
        rule->SaveRam(s);
        const std::string d = s.str();
        return {d.begin(), d.end()};
    }
    void setNvram(const std::vector<std::uint8_t>& d) override {
        auto* rule = core_.GetMemory()->GetCurrentRule();
        if (!rule || d.empty()) return;
        std::istringstream s(std::string(d.begin(), d.end()));
        rule->LoadRam(s, static_cast<s32>(d.size()));
    }

private:
    void set(Gameboy_Keys k, bool on) {
        const int bit = static_cast<int>(k);
        if (on == ((keys_ & bit) != 0)) return;
        keys_ ^= bit;
        if (on) core_.KeyPressed(k);
        else core_.KeyReleased(k);
    }

    GearboyCore core_;
    bool forceDmg_ = false;
    int keys_ = 0;
    u16 fb_[GAMEBOY_WIDTH * GAMEBOY_HEIGHT] = {};
    s16 audio_[AUDIO_BUFFER_SIZE * 2] = {};
    std::vector<std::uint32_t> frame_ = std::vector<std::uint32_t>(GAMEBOY_WIDTH * GAMEBOY_HEIGHT);
    AudioOut out_;
};

}  // namespace

MachinePtr makeGameBoy(const std::uint8_t* rom, std::size_t size, Variant v, std::string* error) {
    auto m = std::make_unique<GameBoyMachine>();
    if (!m->load(rom, size, v == Variant::GameBoy, error)) return nullptr;
    return m;
}

}  // namespace zen1::consoles
