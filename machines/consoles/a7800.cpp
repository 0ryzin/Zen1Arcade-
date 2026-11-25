// SPDX-License-Identifier: GPL-3.0-or-later
//
// Atari 7800 on ProSystem (Greg Stanton, GPL-2.0-or-later; the libretro
// maintainers' version with POKEY, BupChip and the cartridge database; see
// THIRD_PARTY.md). This is Zen1's own glue in place of ProSystem's libretro
// front end. The 7800 BIOS is optional (games start without it); a user's
// "7800 BIOS (U).rom" in ~/Zen1Arcade/BIOS/Atari 7800 is used when present.
#include "cores.hpp"
#include "pad.hpp"

extern "C" {
#include "Bios.h"
#include "BupChip.h"
#include "Cartridge.h"
#include "Database.h"
#include "Maria.h"
#include "Palette.h"
#include "Pokey.h"
#include "ProSystem.h"
#include "Region.h"
#include "Tia.h"
}

#include <algorithm>
#include <cstring>
#include <vector>

namespace zen1::consoles {
namespace {

constexpr int kW = 320, kH = 240;  // canvas; the 7800's picture is 320 x 223 (NTSC)
constexpr size_t kSaveStateSize = 49221;
bool g_inUse = false;  // ProSystem is one global machine

class A7800Machine final : public emu::Machine {
public:
    ~A7800Machine() override {
        prosystem_Close(false);
        bios_Release();
        g_inUse = false;
    }

    bool load(const std::uint8_t* rom, std::size_t size, const std::string& biosDir, std::string* error) {
        if (g_inUse) {
            if (error) *error = "Another Atari 7800 game is already running";
            return false;
        }
        g_inUse = true;
        rom_.assign(rom, rom + size);
        if (!cartridge_Load(false, rom_.data(), static_cast<uint32_t>(rom_.size()))) {
            if (error) *error = "This file is not an Atari 7800 cartridge ProSystem can read";
            return false;
        }
        database_Load(cartridge_digest);
        // The BIOS only shows the Atari logo and checks the cartridge; it is optional.
        bios_enabled = false;
        if (!biosDir.empty()) {
            const std::string name = cartridge_region == REGION_PAL ? "/7800 BIOS (E).rom" : "/7800 BIOS (U).rom";
            if (bios_Load((biosDir + name).c_str())) bios_enabled = true;
        }
        std::memset(keys_, 0, sizeof keys_);
        keys_[15] = 1;  // left difficulty switch: B (beginner)
        keys_[16] = 0;  // right difficulty switch: A (fixes Tower Toppler)
        prosystem_Reset();
        for (int i = 0; i < 256; ++i)
            palette_[i] = std::uint32_t{palette_data[i * 3]} << 16 | std::uint32_t{palette_data[i * 3 + 1]} << 8 |
                          palette_data[i * 3 + 2];
        return true;
    }

    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = kW;
        v.height = kH;
        v.aspect = 4.0 / 3.0;
        v.refreshHz = prosystem_frequency;
        return v;
    }
    int audioSampleRate() const override { return static_cast<int>(tia_size * prosystem_frequency); }
    void reset() override { prosystem_Reset(); }
    void setInput(const emu::CabinetInput& in) override {
        for (int pl = 0; pl < 2; ++pl) {
            const auto& p = in.player[static_cast<size_t>(pl)];
            std::uint8_t* k = keys_ + pl * 6;
            k[0] = p.right;
            k[1] = p.left;
            k[2] = p.down;
            k[3] = p.up;
            // Two fire buttons: the bottom row (or top) left = 1, middle = 2.
            k[4] = consoles::first(p);
            k[5] = consoles::second(p);
        }
        keys_[12] = 0;                                                // console reset
        keys_[13] = consoles::third(in.player[0]);                    // select
        keys_[14] = in.player[0].start || in.player[1].start;         // pause/start
    }
    void runFrame() override {
        prosystem_ExecuteFrame(keys_);
        const uint32_t w = maria_visibleArea.right - maria_visibleArea.left + 1;
        const uint32_t h = maria_visibleArea.bottom - maria_visibleArea.top + 1;
        const std::uint8_t* src = maria_surface + (maria_visibleArea.top - maria_displayArea.top) * w;
        std::fill(frame_.begin(), frame_.end(), 0u);
        const int ox = (kW - static_cast<int>(w)) / 2, oy = (kH - static_cast<int>(h)) / 2;
        for (uint32_t y = 0; y < h; ++y) {
            if (oy + static_cast<int>(y) < 0 || oy + static_cast<int>(y) >= kH) continue;
            for (uint32_t x = 0; x < w && x < static_cast<uint32_t>(kW); ++x)
                frame_[static_cast<size_t>((oy + static_cast<int>(y)) * kW + ox + static_cast<int>(x))] = palette_[src[y * w + x]];
        }
        // TIA (plus POKEY, mixed in, for the cartridges that carry one) -> stereo.
        for (uint32_t i = 0; i < tia_size; ++i) {
            int s = tia_buffer[i];
            if (cartridge_pokey_address) s = (s + pokey_buffer[i]) >> 1;
            // TIA output is 0 (silence) up to its loudest level, never below.
            const auto v = static_cast<std::int16_t>(std::min(s << 7, 32767));
            mix_.push_back(v);
            mix_.push_back(v);
        }
        if (cartridge_bupchip) {  // "Rikki & Vikki": BupChip music, resampled onto the frame
            const size_t n = tia_size, src2 = CORETONE_BUFFER_SAMPLES * 4;
            const size_t base = mix_.size() - n * 2;
            for (size_t t = 0; t < n; ++t) {
                const size_t si = std::min(src2 - 1, t * src2 / n);
                for (int c = 0; c < 2; ++c) {
                    const int v = mix_[base + t * 2 + c] + bupchip_buffer[si * 2 + c];
                    mix_[base + t * 2 + c] = static_cast<std::int16_t>(std::clamp(v, -32768, 32767));
                }
            }
        }
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override {
        out.insert(out.end(), mix_.begin(), mix_.end());
        mix_.clear();
    }
    std::vector<std::uint8_t> saveState() override {
        std::vector<std::uint8_t> d(kSaveStateSize + 1024);
        if (!prosystem_Save(reinterpret_cast<char*>(d.data()), false)) return {};
        return d;
    }
    bool loadState(const std::vector<std::uint8_t>& d) override {
        return d.size() >= kSaveStateSize && prosystem_Load(reinterpret_cast<const char*>(d.data()), false);
    }

private:
    std::vector<std::uint8_t> rom_;
    std::uint8_t keys_[17] = {};
    std::uint32_t palette_[256] = {};
    std::vector<std::uint32_t> frame_ = std::vector<std::uint32_t>(kW * kH, 0);
    std::vector<std::int16_t> mix_;
};

}  // namespace

MachinePtr makeAtari7800(const std::uint8_t* rom, std::size_t size, const std::string& biosDir, std::string* error) {
    auto m = std::make_unique<A7800Machine>();
    if (!m->load(rom, size, biosDir, error)) return nullptr;
    return m;
}

}  // namespace zen1::consoles
