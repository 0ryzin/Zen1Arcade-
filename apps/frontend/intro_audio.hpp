// SPDX-License-Identifier: GPL-3.0-or-later
//
// The intro's sound, synthesised by Zen1 itself (no sample files): a riser,
// one hit per logo letter, an impact, a neon chord pad with an arpeggio and
// a whoosh into the library. It follows the same timeline as Intro.qml,
// scaled to the chosen intro length.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

std::vector<std::int16_t> synthIntroAudio(int rate, double seconds);  // stereo, interleaved
// Short arcade sound effects: move, select, back, error, coin, start, ambience.
std::vector<std::int16_t> synthSfx(const std::string& name, int rate);
bool writeWav(const std::string& path, const std::vector<std::int16_t>& stereo, int rate);

class IntroAudioPlayer {
public:
    ~IntroAudioPlayer();
    bool play(double seconds);
    void fadeOut();
    void stop();

private:
    void* stream_ = nullptr;
};

// Plays the sound effects; overlapping sounds each get their own stream.
class SfxPlayer {
public:
    ~SfxPlayer();
    void play(const std::string& name, float volume = 1.0f);

private:
    std::vector<void*> streams_;
};
