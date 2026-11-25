// SPDX-License-Identifier: GPL-3.0-or-later
//
// NES / Famicom, on Shay Green's Nes_Emu ("QuickNES", LGPL-2.1-or-later,
// as maintained by the libretro project; only the emulation library is
// used, never the libretro API).
#include "cores.hpp"
#include "pad.hpp"

#include "Data_Reader.h"
#include "Nes_Emu.h"

#include <cstring>

namespace zen1::consoles {
namespace {

constexpr int kRate = 44100;
constexpr int kRowBytes = Nes_Emu::image_width + 16;

class NesMachine final : public emu::Machine {
public:
    bool load(const std::uint8_t* rom, std::size_t size, std::string* error) {
        if (const char* e = emu_.set_sample_rate(kRate)) {
            if (error) *error = e;
            return false;
        }
        emu_.set_palette_range(0);
        emu_.set_pixels(pixels_.data(), kRowBytes);
        Mem_File_Reader reader(rom, static_cast<long>(size));
        if (const char* e = emu_.load_ines(reader)) {
            if (error) *error = std::string("QuickNES could not load this cartridge: ") + e;
            return false;
        }
        return true;
    }

    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = Nes_Emu::image_width;
        v.height = Nes_Emu::image_height;
        v.aspect = 4.0 / 3.0;
        v.refreshHz = 60.0988;
        return v;
    }
    int audioSampleRate() const override { return kRate; }
    void reset() override { emu_.reset(false); }

    void setInput(const emu::CabinetInput& in) override {
        for (int j = 0; j < 2; ++j) {
            const auto& p = in.player[static_cast<size_t>(j)];
            int b = 0;
            if (second(p)) b |= 1;   // A
            if (first(p)) b |= 2;    // B
            if (third(p)) b |= 4;    // Select
            if (p.start) b |= 8;     // Start
            if (p.up) b |= 16;
            if (p.down) b |= 32;
            if (p.left) b |= 64;
            if (p.right) b |= 128;
            pads_[j] = b;
        }
    }

    void runFrame() override {
        emu_.emulate_frame(pads_[0], pads_[1]);
        const Nes_Emu::frame_t& f = emu_.frame();
        std::uint32_t pal[256];
        for (int i = 0; i < 256; ++i) {
            const Nes_Emu::rgb_t& c = Nes_Emu::nes_colors[f.palette[i]];
            pal[i] = (static_cast<std::uint32_t>(c.red) << 16) | (static_cast<std::uint32_t>(c.green) << 8) | c.blue;
        }
        for (int y = 0; y < Nes_Emu::image_height; ++y) {
            const std::uint8_t* src = f.pixels + y * kRowBytes;
            std::uint32_t* dst = frame_.data() + y * Nes_Emu::image_width;
            for (int x = 0; x < Nes_Emu::image_width; ++x) dst[x] = pal[src[x]];
        }
        short mono[4096];
        const long n = emu_.read_samples(mono, 4096);
        for (long i = 0; i < n; ++i) {
            audio_.buf.push_back(mono[i]);
            audio_.buf.push_back(mono[i]);
        }
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override { audio_.take(out); }

    std::vector<std::uint8_t> nvram() const override {
        if (!emu_.has_battery_ram()) return {};
        const std::uint8_t* p = const_cast<Nes_Emu&>(emu_).high_mem();
        return {p, p + Nes_Emu::high_mem_size};
    }
    void setNvram(const std::vector<std::uint8_t>& d) override {
        if (emu_.has_battery_ram() && d.size() == Nes_Emu::high_mem_size) std::memcpy(emu_.high_mem(), d.data(), d.size());
    }

private:
    Nes_Emu emu_;
    int pads_[2] = {0, 0};
    std::vector<std::uint8_t> pixels_ = std::vector<std::uint8_t>(static_cast<size_t>(kRowBytes) * (Nes_Emu::image_height + 2));
    std::vector<std::uint32_t> frame_ = std::vector<std::uint32_t>(Nes_Emu::image_width * Nes_Emu::image_height);
    AudioOut audio_;
};

}  // namespace

MachinePtr makeNes(const std::uint8_t* rom, std::size_t size, std::string* error) {
    auto m = std::make_unique<NesMachine>();
    if (!m->load(rom, size, error)) return nullptr;
    return m;
}

}  // namespace zen1::consoles
