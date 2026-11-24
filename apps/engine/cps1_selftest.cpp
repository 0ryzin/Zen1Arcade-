// SPDX-License-Identifier: GPL-3.0-or-later
//
// A CPS1 self-test "game" made entirely by Zen1 (no Capcom data): a tiny
// hand-written 68000 program plus synthetic graphics. It runs the real CPS1
// machine code path (CPU, interrupts, sprite renderer, palette, frame loop,
// audio stream) so the engine can be checked on the build server and on the
// Deck without any ROM files. Selected with --cps1-selftest.
#include "cps1_selftest.hpp"

#include "cps1.hpp"

#include <algorithm>
#include <cmath>

namespace zen1::engine {

namespace {

void put16(std::vector<std::uint8_t>& m, std::uint32_t a, std::uint16_t v) {
    m[a] = static_cast<std::uint8_t>(v >> 8);
    m[a + 1] = static_cast<std::uint8_t>(v);
}
void put32(std::vector<std::uint8_t>& m, std::uint32_t a, std::uint32_t v) {
    put16(m, a, static_cast<std::uint16_t>(v >> 16));
    put16(m, a + 2, static_cast<std::uint16_t>(v));
}

// Sets pixel (x, y) of 16x16 sprite tile `code` to `pen` (4 planes, MSB-left).
void setPixel(std::vector<std::uint8_t>& gfx, int code, int x, int y, int pen) {
    const size_t o = static_cast<size_t>(code) * 128 + static_cast<size_t>(y) * 8 + (x >= 8 ? 4 : 0);
    const int bit = 7 - (x & 7);
    for (int plane = 0; plane < 4; ++plane)
        if (pen & (1 << plane)) gfx[o + static_cast<size_t>(plane)] |= static_cast<std::uint8_t>(1 << bit);
}

class SelfTest final : public emu::Machine {
public:
    SelfTest() {
        emu::RegionMap r;
        r["maincpu"].assign(0x400000, 0);
        r["gfx"].assign(0x600000, 0);
        r["audiocpu"].assign(0x18000, 0);
        r["oki"].assign(0x40000, 0);
        r["audiocpu"][0] = 0x76;  // Z80 HALT
        auto& rom = r["maincpu"];
        put32(rom, 0x000000, 0x00fffff0);
        put32(rom, 0x000004, 0x00000400);
        put32(rom, 0x000068, 0x00000500);
        put16(rom, 0x400, 0x46fc); put16(rom, 0x402, 0x2000);  // MOVE.W #$2000,SR
        put16(rom, 0x404, 0x60fe);                             // BRA.S *
        put16(rom, 0x500, 0x5279); put32(rom, 0x502, 0x00ff8000);  // ADDQ.W #1,$FF8000
        put16(rom, 0x506, 0x4e73);                             // RTE

        // Tile 1: a ring; tile 2: a filled diamond. Pens 1..15 as a gradient.
        auto& gfx = r["gfx"];
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const double dx = x - 7.5, dy = y - 7.5, d = std::sqrt(dx * dx + dy * dy);
                if (d > 4.5 && d < 7.5) setPixel(gfx, 1, x, y, 1 + (x + y) % 15);
                if (std::abs(dx) + std::abs(dy) < 8.0) setPixel(gfx, 2, x, y, 1 + static_cast<int>(d * 2) % 15);
            }
        m_ = std::make_unique<cps1::Cps1Machine>(*cps1::findGameDef("sf2"), std::move(r));

        m_->cpsB()[0x30 / 2] = 0x3f;
        for (int c = 0; c < 8; ++c)
            for (int pen = 1; pen < 16; ++pen) {
                // Neon ramp: cyan -> magenta -> amber across colours.
                const double t = (pen + c * 2) / 30.0;
                const int rr = static_cast<int>(15 * std::clamp(std::sin(t * 6.28) * 0.5 + 0.5, 0.0, 1.0));
                const int gg = static_cast<int>(15 * std::clamp(std::sin(t * 6.28 + 2.1) * 0.5 + 0.5, 0.0, 1.0));
                const int bb = static_cast<int>(15 * std::clamp(std::sin(t * 6.28 + 4.2) * 0.5 + 0.5, 0.0, 1.0));
                m_->write16(0x914000 + static_cast<std::uint32_t>((c * 16 + pen) * 2),
                            static_cast<std::uint16_t>(0xf000 | rr << 8 | gg << 4 | bb));
            }
        m_->write16(0x800100 + 0x0a, 0x9140);
    }

    emu::VideoInfo video() const override { return m_->video(); }
    int audioSampleRate() const override { return m_->audioSampleRate(); }
    void reset() override { m_->reset(); }
    void setInput(const emu::CabinetInput& in) override { in_ = in; }
    const std::uint32_t* frame() const override { return m_->frame(); }
    void takeAudio(std::vector<std::int16_t>& out) override { m_->takeAudio(out); }

    void runFrame() override {
        // Player 1 moves the diamond; buttons change its colour.
        auto& p = in_.player[0];
        px_ = std::clamp(px_ + (p.right ? 3 : 0) - (p.left ? 3 : 0), 0, 384 - 16);
        py_ = std::clamp(py_ + (p.down ? 3 : 0) - (p.up ? 3 : 0), 0, 224 - 16);
        int colour = 0;
        for (int b = 0; b < 6; ++b)
            if (p.button[static_cast<size_t>(b)]) colour = b + 1;

        std::uint32_t a = 0x920000;
        auto sprite = [&](int x, int y, int code, int col) {
            m_->write16(a, static_cast<std::uint16_t>(64 + x));
            m_->write16(a + 2, static_cast<std::uint16_t>(16 + y));
            m_->write16(a + 4, static_cast<std::uint16_t>(code));
            m_->write16(a + 6, static_cast<std::uint16_t>(col));
            a += 8;
        };
        const double t = static_cast<double>(frame_) / 60.0;
        for (int i = 0; i < 24; ++i) {
            const double ang = t * 1.3 + i * 6.2832 / 24.0;
            sprite(184 + static_cast<int>(std::cos(ang) * 150), 104 + static_cast<int>(std::sin(ang * 2) * 80), 1,
                   i % 8);
        }
        sprite(px_, py_, 2, colour);
        m_->write16(a + 6, 0xff00);  // end of list
        m_->runFrame();
        ++frame_;
    }

private:
    std::unique_ptr<cps1::Cps1Machine> m_;
    emu::CabinetInput in_;
    int px_ = 184, py_ = 104;
    long frame_ = 0;
};

}  // namespace

std::unique_ptr<emu::Machine> makeCps1SelfTest() { return std::make_unique<SelfTest>(); }

}  // namespace zen1::engine
