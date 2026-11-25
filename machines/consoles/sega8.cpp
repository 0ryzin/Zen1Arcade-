// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sega Master System, Game Gear and SG-1000, on the Gearsystem core
// (Ignacio Sanchez, GPL-3.0).
#include "cores.hpp"
#include "pad.hpp"

#include "gearsystem.h"

#include <sstream>

// The core's logger stays quiet when this is set (meant for its MCP mode):
// Zen1 keeps the engine's output for its own log.
bool g_mcp_stdio_mode = true;

namespace zen1::consoles {
namespace {

class Sega8Machine final : public emu::Machine {
public:
    bool load(const std::uint8_t* rom, std::size_t size, Variant v, std::string* error) {
        core_.Init(GS_PIXEL_RGB565);
        gg_ = v == Variant::GameGear;
        // The core tells the systems apart by the file name's extension.
        const char* name = gg_ ? "cartridge.gg" : v == Variant::Sg1000 ? "cartridge.sg" : "cartridge.sms";
        if (!core_.LoadROMFromBuffer(rom, static_cast<int>(size), nullptr, name)) {
            if (error) *error = "Gearsystem could not load this cartridge (unsupported mapper or bad dump)";
            return false;
        }
        cw_ = gg_ ? GS_RESOLUTION_GG_WIDTH : GS_RESOLUTION_SMS_WIDTH;
        ch_ = gg_ ? GS_RESOLUTION_GG_HEIGHT : GS_RESOLUTION_SMS_HEIGHT_EXTENDED;
        frame_.assign(static_cast<size_t>(cw_ * ch_), 0u);
        GS_RuntimeInfo info{};
        core_.GetRuntimeInfo(info);
        fps_ = info.fps > 1 ? info.fps : 59.92;
        return true;
    }

    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = cw_;
        v.height = ch_;
        v.aspect = gg_ ? 10.0 / 9.0 : 4.0 / 3.0;
        v.refreshHz = fps_;
        return v;
    }
    int audioSampleRate() const override { return GS_AUDIO_SAMPLE_RATE; }
    void reset() override { core_.ResetROMPreservingRAM(); }

    void setInput(const emu::CabinetInput& in) override {
        for (int j = 0; j < (gg_ ? 1 : 2); ++j) {
            const auto& p = in.player[static_cast<size_t>(j)];
            const auto pad = j ? Joypad_2 : Joypad_1;
            set(pad, Key_Up, p.up);
            set(pad, Key_Down, p.down);
            set(pad, Key_Left, p.left);
            set(pad, Key_Right, p.right);
            set(pad, Key_1, first(p));
            set(pad, Key_2, second(p));
            set(pad, Key_Start, p.start);  // Game Gear START; Master System PAUSE
        }
    }

    void runFrame() override {
        int samples = 0;
        core_.RunToVBlank(reinterpret_cast<u8*>(fb_.data()), audio_, &samples);
        GS_RuntimeInfo info{};
        core_.GetRuntimeInfo(info);
        const int w = std::min(info.screen_width, cw_), h = std::min(info.screen_height, ch_);
        conv_.resize(static_cast<size_t>(w * h));
        for (size_t i = 0; i < conv_.size(); ++i) conv_[i] = rgb565(fb_[i]);
        blit(frame_, cw_, ch_, conv_.data(), w, h, w);
        out_.stereo(audio_, samples / 2);
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override { out_.take(out); }

    std::vector<std::uint8_t> nvram() const override {
        auto* rule = const_cast<GearsystemCore&>(core_).GetMemory()->GetCurrentRule();
        if (!rule || !rule->PersistedRAM() || rule->GetRamSize() == 0) return {};
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
    void set(GS_Joypads pad, GS_Keys k, bool on) {
        int& keys = keys_[pad == Joypad_2 ? 1 : 0];
        const int bit = static_cast<int>(k);
        if (on == ((keys & bit) != 0)) return;
        keys ^= bit;
        if (on) core_.KeyPressed(pad, k);
        else core_.KeyReleased(pad, k);
    }

    GearsystemCore core_;
    bool gg_ = false;
    int cw_ = 256, ch_ = 224;
    double fps_ = 59.92;
    int keys_[2] = {0, 0};
    std::vector<u16> fb_ = std::vector<u16>(GS_RESOLUTION_MAX_WIDTH_WITH_OVERSCAN * GS_RESOLUTION_MAX_HEIGHT_WITH_OVERSCAN);
    std::vector<std::uint32_t> conv_, frame_;
    s16 audio_[GS_AUDIO_BUFFER_SIZE_V1 * 2] = {};
    AudioOut out_;
};

}  // namespace

MachinePtr makeSega8(const std::uint8_t* rom, std::size_t size, Variant v, std::string* error) {
    auto m = std::make_unique<Sega8Machine>();
    if (!m->load(rom, size, v, error)) return nullptr;
    return m;
}

}  // namespace zen1::consoles
