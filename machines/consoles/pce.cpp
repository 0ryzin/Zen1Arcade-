// SPDX-License-Identifier: GPL-3.0-or-later
//
// PC Engine / TurboGrafx-16 HuCards, on the Geargrafx core (Ignacio
// Sanchez, GPL-3.0). CD-ROM games need a System Card BIOS, which Zen1 does
// not ship; they are not offered.
#include "cores.hpp"
#include "pad.hpp"

#include "geargrafx.h"

#include <cstring>

// The core's logger stays quiet when this is set (meant for its MCP mode):
// Zen1 keeps the engine's output for its own log.
bool g_mcp_stdio_mode = true;

namespace zen1::consoles {
namespace {

class PcEngineMachine;
PcEngineMachine* g_current = nullptr;  // the core's input pump has no user data
void pump();

// The picture is drawn into a fixed 512-pixel-wide canvas: 256- and
// 512-pixel modes fit exactly, other widths are stretched to it.
constexpr int kCanvasW = 512, kCanvasH = 242;

class PcEngineMachine final : public emu::Machine {
public:
    ~PcEngineMachine() override {
        if (g_current == this) g_current = nullptr;
    }
    bool load(const std::uint8_t* rom, std::size_t size, std::string* error) {
        g_current = this;
        core_.Init(pump, GG_PIXEL_RGB565);
        if (!core_.LoadHuCardFromBuffer(rom, static_cast<int>(size), "hucard.pce")) {
            if (error) *error = "Geargrafx could not load this HuCard";
            return false;
        }
        return true;
    }

    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = kCanvasW;
        v.height = kCanvasH;
        v.aspect = 4.0 / 3.0;
        v.refreshHz = 59.826;
        return v;
    }
    int audioSampleRate() const override { return GG_AUDIO_SAMPLE_RATE; }
    void reset() override { core_.ResetMedia(true); }
    void setInput(const emu::CabinetInput& in) override { in_ = in; }

    void applyInput() {
        for (int j = 0; j < 2; ++j) {
            const auto& p = in_.player[static_cast<size_t>(j)];
            const auto c = j ? GG_CONTROLLER_2 : GG_CONTROLLER_1;
            set(c, j, GG_KEY_UP, p.up);
            set(c, j, GG_KEY_DOWN, p.down);
            set(c, j, GG_KEY_LEFT, p.left);
            set(c, j, GG_KEY_RIGHT, p.right);
            set(c, j, GG_KEY_II, first(p));
            set(c, j, GG_KEY_I, second(p));
            set(c, j, GG_KEY_SELECT, third(p));
            set(c, j, GG_KEY_RUN, p.start);
        }
    }

    void runFrame() override {
        g_current = this;
        int samples = 0;
        core_.RunToVBlank(reinterpret_cast<u8*>(fb_.data()), audio_, &samples);
        GG_Runtime_Info info{};
        core_.GetRuntimeInfo(info);
        const int w = std::max(1, std::min(info.screen_width, 2048));
        const int h = std::min(info.screen_height, kCanvasH);
        std::fill(frame_.begin(), frame_.end(), 0u);
        const int oy = (kCanvasH - h) / 2;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < kCanvasW; ++x)
                frame_[static_cast<size_t>((oy + y) * kCanvasW + x)] = rgb565(fb_[static_cast<size_t>(y * w + x * w / kCanvasW)]);
        out_.stereo(audio_, samples / 2);
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override { out_.take(out); }

    std::vector<std::uint8_t> nvram() const override {
        auto* mem = const_cast<GeargrafxCore&>(core_).GetMemory();
        if (!mem->IsBackupRamUsed()) return {};
        const u8* p = mem->GetBackupRAM();
        return {p, p + mem->GetBackupRAMSize()};
    }
    void setNvram(const std::vector<std::uint8_t>& d) override {
        auto* mem = core_.GetMemory();
        if (d.size() == static_cast<size_t>(mem->GetBackupRAMSize())) std::memcpy(mem->GetBackupRAM(), d.data(), d.size());
    }

private:
    void set(GG_Controllers c, int j, GG_Keys k, bool on) {
        int& keys = keys_[j];
        const int bit = static_cast<int>(k);
        if (on == ((keys & bit) != 0)) return;
        keys ^= bit;
        if (on) core_.KeyPressed(c, k);
        else core_.KeyReleased(c, k);
    }

    GeargrafxCore core_;
    emu::CabinetInput in_;
    int keys_[2] = {0, 0};
    std::vector<u16> fb_ = std::vector<u16>(2048 * 512);
    std::vector<std::uint32_t> frame_ = std::vector<std::uint32_t>(kCanvasW * kCanvasH);
    s16 audio_[GG_AUDIO_BUFFER_SIZE * 4] = {};
    AudioOut out_;
};

void pump() {
    if (g_current) g_current->applyInput();
}

}  // namespace

MachinePtr makePcEngine(const std::uint8_t* rom, std::size_t size, std::string* error) {
    auto m = std::make_unique<PcEngineMachine>();
    if (!m->load(rom, size, error)) return nullptr;
    return m;
}

}  // namespace zen1::consoles
