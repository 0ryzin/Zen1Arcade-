// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sega Mega Drive / Genesis cartridges, on clownmdemu (Clownacy,
// AGPL-3.0-or-later; combined with Zen1 under GPL-3.0 section 13, see
// THIRD_PARTY.md). Mega CD and 32X are not offered.
#include "cores.hpp"
#include "pad.hpp"

#include "clownmdemu.h"  // C, with its own C++ guards

#include <algorithm>
#include <cmath>
#include <cstring>

namespace zen1::consoles {
namespace {

constexpr int kRate = 48000;
constexpr int kW = 320, kH = 240;

class MegaDriveMachine final : public emu::Machine {
public:
    bool load(const std::uint8_t* rom, std::size_t size, std::string* error) {
        // .smd (Super Magic Drive copier) dumps: a 512-byte header, then 16 KiB
        // blocks holding the odd bytes in the first half and the even bytes in
        // the second. Recognised by layout and the copier's 0xAA 0xBB marker.
        if (size > 0x200 && (size - 0x200) % 0x4000 == 0 && rom[8] == 0xAA && rom[9] == 0xBB) {
            smd_.resize(size - 0x200);
            for (size_t b = 0; b < smd_.size(); b += 0x4000) {
                const std::uint8_t* blk = rom + 0x200 + b;
                for (size_t i = 0; i < 0x2000; ++i) {
                    smd_[b + 2 * i] = blk[0x2000 + i];
                    smd_[b + 2 * i + 1] = blk[i];
                }
            }
            rom = smd_.data();
            size = smd_.size();
        }
        if (size < 0x200) {
            if (error) *error = "This file is too small to be a Mega Drive cartridge";
            return false;
        }
        // Words, big-endian, as the 68000 sees the cartridge.
        rom_.resize((size + 1) / 2);
        for (size_t i = 0; i < rom_.size(); ++i)
            rom_[i] = static_cast<cc_u16l>((rom[2 * i] << 8) | (2 * i + 1 < size ? rom[2 * i + 1] : 0));
        // Region from the header (0x1F0): Europe only -> PAL; Japan only -> domestic.
        std::string region(reinterpret_cast<const char*>(rom) + 0x1f0, 3);
        const bool j = region.find('J') != std::string::npos, u = region.find('U') != std::string::npos,
                   e = region.find('E') != std::string::npos;
        pal_ = e && !u && !j;

        static bool constants = [] { ClownMDEmu_Constant_Initialise(); return true; }();
        (void)constants;
        std::memset(&config_, 0, sizeof config_);
        config_.general.region = (j && !u && !e) ? CLOWNMDEMU_REGION_DOMESTIC : CLOWNMDEMU_REGION_OVERSEAS;
        config_.general.tv_standard = pal_ ? CLOWNMDEMU_TV_STANDARD_PAL : CLOWNMDEMU_TV_STANDARD_NTSC;
        std::memset(&callbacks_, 0, sizeof callbacks_);
        callbacks_.user_data = this;
        callbacks_.colour_updated = colourUpdated;
        callbacks_.scanline_rendered = scanlineRendered;
        callbacks_.input_requested = inputRequested;
        callbacks_.fm_audio_to_be_generated = fmAudio;
        callbacks_.psg_audio_to_be_generated = psgAudio;
        callbacks_.pcm_audio_to_be_generated = pcmAudio;
        callbacks_.cdda_audio_to_be_generated = cddaAudio;
        callbacks_.cd_seeked = [](void*, cc_u32f) {};
        callbacks_.cd_sector_read = [](void*, cc_u16l*) {};
        callbacks_.cd_track_seeked = [](void*, cc_u16f, ClownMDEmu_CDDAMode) -> cc_bool { return cc_false; };
        callbacks_.cd_audio_read = [](void*, cc_s16l*, size_t) -> size_t { return 0; };
        callbacks_.save_file_opened_for_reading = [](void*, const char*) -> cc_bool { return cc_false; };
        callbacks_.save_file_read = [](void*) -> cc_s16f { return -1; };
        callbacks_.save_file_opened_for_writing = [](void*, const char*) -> cc_bool { return cc_false; };
        callbacks_.save_file_written = [](void*, cc_u8f) {};
        callbacks_.save_file_closed = [](void*) {};
        callbacks_.save_file_removed = [](void*, const char*) -> cc_bool { return cc_false; };
        callbacks_.save_file_size_obtained = [](void*, const char*, size_t*) -> cc_bool { return cc_false; };
        emu_ = std::make_unique<ClownMDEmu>();
        ClownMDEmu_Initialise(emu_.get(), &config_, &callbacks_);
        ClownMDEmu_SetCartridge(emu_.get(), rom_.data(), static_cast<cc_u32f>(rom_.size()));
        ClownMDEmu_HardReset(emu_.get(), cc_true, cc_false);
        return true;
    }

    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = kW;
        v.height = kH;
        v.aspect = 4.0 / 3.0;
        v.refreshHz = pal_ ? 49.701 : 59.922;
        return v;
    }
    int audioSampleRate() const override { return kRate; }
    void reset() override { ClownMDEmu_SoftReset(emu_.get(), cc_true, cc_false); }
    void setInput(const emu::CabinetInput& in) override { in_ = in; }

    void runFrame() override {
        fm_.clear();
        psg_.clear();
        std::fill(frame_.begin(), frame_.end(), 0u);
        ClownMDEmu_Iterate(emu_.get());
        mixAudio();
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override { audio_.take(out); }

    // Battery-backed cartridge SRAM (save games), when the header declares it.
    std::vector<std::uint8_t> nvram() const override {
        const auto& r = emu_->state.external_ram;
        if (r.size == 0 || r.size > sizeof r.buffer) return {};
        return std::vector<std::uint8_t>(r.buffer, r.buffer + r.size);
    }
    void setNvram(const std::vector<std::uint8_t>& d) override {
        auto& r = emu_->state.external_ram;
        if (r.size == 0) return;
        std::memcpy(r.buffer, d.data(), std::min<size_t>(d.size(), r.size));
    }

private:
    static void colourUpdated(void* u, cc_u16f index, cc_u16f c) {
        auto* m = static_cast<MegaDriveMachine*>(u);
        const std::uint32_t r = c & 0xf, g = (c >> 4) & 0xf, b = (c >> 8) & 0xf;
        if (index < 256) m->palette_[index] = (r * 17) << 16 | (g * 17) << 8 | (b * 17);
    }
    static void scanlineRendered(void* u, cc_u16f line, const cc_u8l* px, cc_u16f left, cc_u16f right, cc_u16f sw, cc_u16f sh) {
        auto* m = static_cast<MegaDriveMachine*>(u);
        const int y = static_cast<int>(line) + (kH - static_cast<int>(sh)) / 2;
        if (y < 0 || y >= kH || sw == 0) return;
        std::uint32_t* dst = m->frame_.data() + y * kW;
        // 256-pixel (H32) modes are stretched to the full width, as a TV showed them.
        for (int x = 0; x < kW; ++x) {
            const int sx = x * static_cast<int>(sw) / kW;
            if (sx < static_cast<int>(left) || sx >= static_cast<int>(right)) continue;
            dst[x] = m->palette_[px[sx] & 0xff];
        }
    }
    static cc_bool inputRequested(void* u, cc_u8f player, ClownMDEmu_Button b) {
        auto* m = static_cast<MegaDriveMachine*>(u);
        if (player > 1) return cc_false;
        const auto& p = m->in_.player[player];
        switch (b) {
        case CLOWNMDEMU_BUTTON_UP: return p.up;
        case CLOWNMDEMU_BUTTON_DOWN: return p.down;
        case CLOWNMDEMU_BUTTON_LEFT: return p.left;
        case CLOWNMDEMU_BUTTON_RIGHT: return p.right;
        // Panel: X Y Z (top row) / A B C (bottom row), the six-button pad's face.
        case CLOWNMDEMU_BUTTON_A: return held(p, LK);
        case CLOWNMDEMU_BUTTON_B: return held(p, MK);
        case CLOWNMDEMU_BUTTON_C: return held(p, HK);
        case CLOWNMDEMU_BUTTON_X: return held(p, LP);
        case CLOWNMDEMU_BUTTON_Y: return held(p, MP);
        case CLOWNMDEMU_BUTTON_Z: return held(p, HP);
        case CLOWNMDEMU_BUTTON_START: return p.start;
        default: return cc_false;
        }
    }
    static void fmAudio(void* u, ClownMDEmu* e, size_t n, void (*gen)(ClownMDEmu*, cc_s16l*, size_t)) {
        auto* m = static_cast<MegaDriveMachine*>(u);
        const size_t at = m->fm_.size();
        m->fm_.resize(at + n * 2);
        gen(e, m->fm_.data() + at, n);
    }
    static void psgAudio(void* u, ClownMDEmu* e, size_t n, void (*gen)(ClownMDEmu*, cc_s16l*, size_t)) {
        auto* m = static_cast<MegaDriveMachine*>(u);
        const size_t at = m->psg_.size();
        m->psg_.resize(at + n);
        gen(e, m->psg_.data() + at, n);
    }
    static void pcmAudio(void* u, ClownMDEmu* e, size_t n, void (*gen)(ClownMDEmu*, cc_s16l*, size_t)) {
        auto* m = static_cast<MegaDriveMachine*>(u);
        m->scratch_.resize(n * 2);
        gen(e, m->scratch_.data(), n);
    }
    static void cddaAudio(void* u, ClownMDEmu* e, size_t n, void (*gen)(ClownMDEmu*, cc_s16l*, size_t)) {
        pcmAudio(u, e, n, gen);
    }

    // FM (stereo, ~53 kHz) and PSG (mono, ~224 kHz) -> 48 kHz stereo: this
    // frame's share of output samples, FM interpolated, PSG box-filtered.
    void mixAudio() {
        acc_ += kRate / (pal_ ? 49.701 : 59.922);
        const int n = static_cast<int>(acc_);
        acc_ -= n;
        const size_t fmN = fm_.size() / 2, psgN = psg_.size();
        for (int i = 0; i < n; ++i) {
            double l = 0, r = 0;
            if (fmN) {
                const double pos = (i + 0.5) * fmN / n - 0.5;
                const size_t a = static_cast<size_t>(std::max(0.0, std::floor(pos)));
                const size_t b = std::min(fmN - 1, a + 1);
                const double t = std::clamp(pos - std::floor(pos), 0.0, 1.0);
                l = fm_[a * 2] * (1 - t) + fm_[b * 2] * t;
                r = fm_[a * 2 + 1] * (1 - t) + fm_[b * 2 + 1] * t;
            }
            if (psgN) {
                const size_t a = i * psgN / n, b = std::max(a + 1, (i + 1) * psgN / n);
                double s = 0;
                for (size_t k = a; k < b && k < psgN; ++k) s += psg_[k];
                s /= static_cast<double>(b - a);
                l += s / CLOWNMDEMU_PSG_VOLUME_DIVISOR;
                r += s / CLOWNMDEMU_PSG_VOLUME_DIVISOR;
            }
            // DC blocker: the FM output rests on an offset.
            const double hl = l - dcInL_ + 0.9985 * dcOutL_, hr = r - dcInR_ + 0.9985 * dcOutR_;
            dcInL_ = l; dcInR_ = r; dcOutL_ = hl; dcOutR_ = hr;
            audio_.buf.push_back(static_cast<std::int16_t>(std::clamp(hl, -32768.0, 32767.0)));
            audio_.buf.push_back(static_cast<std::int16_t>(std::clamp(hr, -32768.0, 32767.0)));
        }
    }

    std::vector<cc_u16l> rom_;
    bool pal_ = false;
    ClownMDEmu_InitialConfiguration config_;
    ClownMDEmu_Callbacks callbacks_;
    std::unique_ptr<ClownMDEmu> emu_;
    std::vector<std::uint8_t> smd_;  // a de-interleaved .smd dump
    emu::CabinetInput in_;
    std::uint32_t palette_[256] = {};
    std::vector<std::uint32_t> frame_ = std::vector<std::uint32_t>(kW * kH);
    std::vector<cc_s16l> fm_, psg_, scratch_;
    double acc_ = 0;
    double dcInL_ = 0, dcInR_ = 0, dcOutL_ = 0, dcOutR_ = 0;
    AudioOut audio_;
};

}  // namespace

MachinePtr makeMegaDrive(const std::uint8_t* rom, std::size_t size, std::string* error) {
    auto m = std::make_unique<MegaDriveMachine>();
    if (!m->load(rom, size, error)) return nullptr;
    return m;
}

}  // namespace zen1::consoles
