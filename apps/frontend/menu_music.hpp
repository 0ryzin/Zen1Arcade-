// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zen1's own menu music, "Night Signal", composed for Zen1 and synthesised
// at start-up (no sample files, no borrowed melodies): a 40 s, 16-bar loop
// in D minor at 96 BPM - breathing pads, a pulsing bass, a plucked
// arpeggio whose filter opens and closes over the loop, bell calls, then a
// lead melody over a soft beat. Rendered into a circular buffer (tails,
// echo and reverb wrap round), so it repeats without a gap or click.
#pragma once

#include <cstddef>
#include <cstdint>
#include <future>
#include <vector>

std::vector<std::int16_t> synthMenuMusic(int rate);  // stereo, interleaved, exactly one loop

class MusicPlayer {
public:
    ~MusicPlayer();
    // Renders the loop on a worker thread (about 1-2 s), so the first play()
    // does not stall the UI. play() waits for it if it is still running.
    void prepare();
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
    std::future<std::vector<std::int16_t>> rendering_;
};
