// SPDX-License-Identifier: GPL-3.0-or-later
//
// The contract every Zen1 machine module (CPS1 first) implements. The engine
// runner only talks to this interface, so new hardware never changes it.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zen1::emu {

// Zen1's logical cabinet layout for one player.
struct PlayerInput {
    bool up = false, down = false, left = false, right = false;
    std::array<bool, 6> button{};  // LP MP HP / LK MK HK (button 1-6)
    bool start = false;            // the dedicated player button
    bool coin = false;
};

struct CabinetInput {
    std::array<PlayerInput, 2> player;
    bool service = false;
};

struct VideoInfo {
    int width = 0;           // native frame size in pixels
    int height = 0;
    double aspect = 4.0 / 3.0;  // shape the monitor showed (unrotated)
    int rotation = 0;        // degrees clockwise needed for an upright picture
    double refreshHz = 60.0;
};

class Machine {
public:
    virtual ~Machine() = default;

    virtual VideoInfo video() const = 0;
    virtual int audioSampleRate() const = 0;

    virtual void reset() = 0;
    virtual void setInput(const CabinetInput& in) = 0;

    // Emulates one video frame.
    virtual void runFrame() = 0;

    // XRGB8888 pixels, video().width * video().height, valid after runFrame().
    virtual const std::uint32_t* frame() const = 0;

    // Appends stereo interleaved samples produced since the last call.
    virtual void takeAudio(std::vector<std::int16_t>& out) = 0;

    // Battery-backed memory (high scores, settings), kept in ~/Zen1Arcade/Saves.
    virtual std::vector<std::uint8_t> nvram() const { return {}; }
    virtual void setNvram(const std::vector<std::uint8_t>&) {}
};

}  // namespace zen1::emu
