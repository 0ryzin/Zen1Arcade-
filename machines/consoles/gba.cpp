// SPDX-License-Identifier: GPL-3.0-or-later
//
// Nintendo Game Boy Advance on mGBA (Vicki Pfau and contributors, MPL-2.0;
// see THIRD_PARTY.md). mGBA's own high-level BIOS is used, so no BIOS file
// is needed; a user's gba_bios.bin in ~/Zen1Arcade/BIOS/Game Boy Advance is
// used when present. Battery saves go to ~/Zen1Arcade/Saves/<id>.sav.
#include "cores.hpp"
#include "pad.hpp"

extern "C" {
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/serialize.h>
#include <mgba/gba/core.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/vfs.h>
}

#include <cstring>
#include <fcntl.h>
#include <vector>

namespace zen1::consoles {
namespace {

constexpr int kW = 240, kH = 160;

// GBA key bits.
enum : std::uint32_t { kA = 1, kB = 2, kSelect = 4, kStart = 8, kRight = 16, kLeft = 32, kUp = 64, kDown = 128, kR = 256, kL = 512 };

class GbaMachine final : public emu::Machine {
public:
    ~GbaMachine() override {
        if (core_) {
            core_->unloadROM(core_);  // closes the save file (written as the game saves)
            mCoreConfigDeinit(&core_->config);
            core_->deinit(core_);
        }
    }

    bool load(const std::uint8_t* rom, std::size_t size, const GbaFiles& files, std::string* error) {
        if (size < 0xc0 || size > 32 * 1024 * 1024) {
            if (error) *error = "This file is not a Game Boy Advance cartridge";
            return false;
        }
        rom_.assign(rom, rom + size);
        core_ = GBACoreCreate();
        if (!core_ || !core_->init(core_)) {
            if (error) *error = "mGBA could not start";
            core_ = nullptr;
            return false;
        }
        mCoreInitConfig(core_, nullptr);
        // Defaults a frontend gives mGBA (unset, the volume would be 0).
        mCoreConfigSetDefaultIntValue(&core_->config, "volume", 0x100);
        mCoreConfigSetDefaultIntValue(&core_->config, "skipBios", 1);
        mCoreConfigSetDefaultIntValue(&core_->config, "useBios", files.bios.empty() ? 0 : 1);
        mCoreLoadConfig(core_);
        core_->setVideoBuffer(core_, video_, kW);
        core_->setAudioBufferSize(core_, 4096);
        VFile* vf = VFileFromConstMemory(rom_.data(), rom_.size());
        if (!vf || !core_->loadROM(core_, vf)) {
            if (error) *error = "mGBA could not load this cartridge";
            return false;
        }
        if (!files.bios.empty()) {
            if (VFile* bios = VFileOpen(files.bios.c_str(), O_RDONLY)) core_->loadBIOS(core_, bios, 0);
        }
        if (!files.save.empty()) {
            if (VFile* save = VFileOpen(files.save.c_str(), O_CREAT | O_RDWR)) core_->loadSave(core_, save);
        }
        core_->reset(core_);
        return true;
    }

    emu::VideoInfo video() const override {
        emu::VideoInfo v;
        v.width = kW;
        v.height = kH;
        v.aspect = 3.0 / 2.0;
        v.refreshHz = 16777216.0 / 280896.0;  // 59.73 Hz
        return v;
    }
    int audioSampleRate() const override { return static_cast<int>(core_->audioSampleRate(core_)); }
    void reset() override { core_->reset(core_); }
    void setInput(const emu::CabinetInput& in) override {
        const auto& p = in.player[0];
        std::uint32_t k = 0;
        if (p.up) k |= kUp;
        if (p.down) k |= kDown;
        if (p.left) k |= kLeft;
        if (p.right) k |= kRight;
        if (p.start) k |= kStart;
        // Panel: bottom row B A, top row L R, HP / HK = Select.
        if (held(p, LK)) k |= kB;
        if (held(p, MK)) k |= kA;
        if (held(p, LP)) k |= kL;
        if (held(p, MP)) k |= kR;
        if (held(p, HP) || held(p, HK)) k |= kSelect;
        core_->setKeys(core_, k);
    }
    void runFrame() override {
        core_->runFrame(core_);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x) {
                const std::uint32_t c = video_[y * kW + x];  // mGBA: 0x00BBGGRR
                frame_[static_cast<size_t>(y * kW + x)] = (c & 0xff) << 16 | (c & 0xff00) | (c >> 16 & 0xff);
            }
        mAudioBuffer* ab = core_->getAudioBuffer(core_);
        const size_t n = mAudioBufferAvailable(ab);
        if (n) {
            const size_t at = audio_.size();
            audio_.resize(at + n * 2);
            const size_t got = mAudioBufferRead(ab, audio_.data() + at, n);
            audio_.resize(at + got * 2);
        }
    }
    const std::uint32_t* frame() const override { return frame_.data(); }
    void takeAudio(std::vector<std::int16_t>& out) override {
        out.insert(out.end(), audio_.begin(), audio_.end());
        audio_.clear();
    }
    std::vector<std::uint8_t> saveState() override {
        VFile* vf = VFileMemChunk(nullptr, 0);
        std::vector<std::uint8_t> out;
        if (mCoreSaveStateNamed(core_, vf, 0)) {
            out.resize(static_cast<size_t>(vf->size(vf)));
            vf->seek(vf, 0, SEEK_SET);
            vf->read(vf, out.data(), out.size());
        }
        vf->close(vf);
        return out;
    }
    bool loadState(const std::vector<std::uint8_t>& d) override {
        VFile* vf = VFileFromConstMemory(d.data(), d.size());
        const bool ok = vf && mCoreLoadStateNamed(core_, vf, 0);
        if (vf) vf->close(vf);
        return ok;
    }

private:
    mCore* core_ = nullptr;
    std::vector<std::uint8_t> rom_;
    std::uint32_t video_[kW * kH] = {};
    std::vector<std::uint32_t> frame_ = std::vector<std::uint32_t>(kW * kH, 0);
    std::vector<std::int16_t> audio_;
};

}  // namespace

MachinePtr makeGba(const std::uint8_t* rom, std::size_t size, const GbaFiles& files, std::string* error) {
    auto m = std::make_unique<GbaMachine>();
    if (!m->load(rom, size, files, error)) return nullptr;
    return m;
}

}  // namespace zen1::consoles
