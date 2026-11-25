// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zen1's own menu music, composed for Zen1 and synthesised at start-up (no
// sample files, no borrowed melodies): a 16-bar chiptune loop at 120 BPM
// with a pulse bass, a 16th-note arpeggio with echo, a pulse lead in the
// second half, noise drums and the odd arcade bleep. The loop is rendered
// with its own wrap-around tails, so it repeats without a gap or click.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

std::vector<std::int16_t> synthMenuMusic(int rate);  // stereo, interleaved, exactly one loop

class MusicPlayer {
public:
    ~MusicPlayer();
    // Starts (or keeps) the loop and fades to `gain` over `fadeMs`.
    bool play(float gain, int fadeMs = 600);
    void setGain(float gain, int fadeMs = 250);
    void fadeOut(int fadeMs = 500);  // fades to silence, then pauses the device
    bool playing() const;

    // Mixer state (shared with the audio callback).
    struct State {
        std::vector<std::int16_t> pcm;
        std::size_t pos = 0;
        float gain = 0.0f, target = 0.0f, step = 0.0f;
    };

private:
    void* stream_ = nullptr;
    State* state_ = nullptr;
};
