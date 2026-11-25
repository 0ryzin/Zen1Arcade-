// SPDX-License-Identifier: GPL-3.0-or-later
//
// SNES / Super Famicom, on the LakeSnes core (angelo_wf and contributors,
// MIT). LakeSnes runs the base console: cartridges with extra chips
// (Super FX, SA-1, DSP-n, S-DD1...) do not run.
#include "cores.hpp"
#include "pad.hpp"

extern "C" {
#include "snes.h"
#include "ppu.h"
}

#include <cstring>

namespace zen1::consoles {
namespace {

constexpr int kRate = 48000;
constexpr int kW = 256, kH = 224;

class SnesMachine final : public emu::Machine {
public:
    ~SnesMachine() override {
        if (snes_) snes_free(snes_);
    }
    bool load(const std::uint8_t* rom, std::size_t size, std::string* error) {
        snes_ = snes_init();
        snes_setPixelFormat(snes_, 0);  // bytes B, G, R, X: 0x00RRGGBB as little-endian words
        if (!snes_loadRom(snes_, rom, static_cast<int>(size))) {
            if (error) *error = "LakeSnes could not load this cartridge";
            return false;
        }
        pal_ = snes_->palTiming;
        return true;
    }

    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = kW;
        v.height = kH;
        v.aspect = 4.0 / 3.0;
        v.refreshHz = pal_ ? 50.007 : 60.0988;
        return v;
    }
    int audioSampleRate() const override { return kRate; }
    void reset() override { snes_reset(snes_, false); }

    void setInput(const emu::CabinetInput& in) override {
        // LakeSnes buttons: B Y Select Start Up Down Left Right A X L R
        // Panel: Y X L (top row) / B A R (bottom row), like a SNES pad's face.
        for (int j = 0; j < 2; ++j) {
            const auto& p = in.player[static_cast<size_t>(j)];
            const bool b[12] = {held(p, LK), held(p, LP), false,    p.start,    p.up,       p.down,
                                p.left,      p.right,     held(p, MK), held(p, MP), held(p, HP), held(p, HK)};
            for (int k = 0; k < 12; ++k) snes_setButtonState(snes_, j + 1, k, b[k]);
        }
    }

    void runFrame() override {
        snes_runFrame(snes_);
        snes_setPixels(snes_, pixels_.data());
        // 512 x 480 doubled picture -> 256 x 224: visible lines start at row
        // 16; the two half-pixels of a dot are averaged (keeps hi-res text).
        const auto* src = reinterpret_cast<const std::uint32_t*>(pixels_.data());
        for (int y = 0; y < kH; ++y) {
            const std::uint32_t* row = src + (16 + 2 * y) * 512;
            for (int x = 0; x < kW; ++x) {
                const std::uint32_t a = row[2 * x] & 0xfefefe, b = row[2 * x + 1] & 0xfefefe;
                frame_[static_cast<size_t>(y * kW + x)] = (a >> 1) + (b >> 1);
            }
        }
        // Samples for exactly this frame's share of the output rate.
        acc_ += kRate / (pal_ ? 50.007 : 60.0988);
        const int n = static_cast<int>(acc_);
        acc_ -= n;
        samples_.resize(static_cast<size_t>(n) * 2);
        snes_setSamples(snes_, samples_.data(), n);
        audio_.stereo(samples_.data(), n);
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override { audio_.take(out); }

    std::vector<std::uint8_t> nvram() const override {
        const int size = snes_saveBattery(snes_, nullptr);
        if (size <= 0) return {};
        std::vector<std::uint8_t> d(static_cast<size_t>(size));
        snes_saveBattery(snes_, d.data());
        return d;
    }
    void setNvram(const std::vector<std::uint8_t>& d) override {
        if (d.empty()) return;
        std::vector<std::uint8_t> copy = d;
        snes_loadBattery(snes_, copy.data(), static_cast<int>(copy.size()));
    }

private:
    Snes* snes_ = nullptr;
    bool pal_ = false;
    double acc_ = 0;
    std::vector<std::uint8_t> pixels_ = std::vector<std::uint8_t>(512 * 480 * 4);
    std::vector<std::uint32_t> frame_ = std::vector<std::uint32_t>(kW * kH);
    std::vector<std::int16_t> samples_;
    AudioOut audio_;
};

}  // namespace

MachinePtr makeSnes(const std::uint8_t* rom, std::size_t size, std::string* error) {
    auto m = std::make_unique<SnesMachine>();
    if (!m->load(rom, size, error)) return nullptr;
    return m;
}

}  // namespace zen1::consoles
