// SPDX-License-Identifier: GPL-3.0-or-later
#include "intro_audio.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Buf {
    int rate;
    std::vector<double> l, r;
    Buf(int rate_, double seconds)
        : rate(rate_), l(static_cast<size_t>(rate_ * seconds) + 1, 0.0), r(l.size(), 0.0) {}
    size_t at(double t) const { return std::min(l.size() - 1, static_cast<size_t>(std::max(0.0, t) * rate)); }
};

double env(double t, double a, double d) {  // attack then exponential decay
    if (t < 0) return 0;
    return t < a ? t / a : std::exp(-(t - a) / d);
}

// Kick-like hit: pitch-dropping sine with a noise click.
void hit(Buf& b, double t0, double gain, double pan, std::mt19937& rng) {
    std::uniform_real_distribution<double> n(-1, 1);
    double ph = 0;
    for (size_t i = b.at(t0); i < b.at(t0 + 0.6); ++i) {
        const double t = static_cast<double>(i) / b.rate - t0;
        const double f = 40 + 140 * std::exp(-t * 28);
        ph += 2 * kPi * f / b.rate;
        const double v = (std::sin(ph) * env(t, 0.002, 0.18) + n(rng) * env(t, 0.001, 0.012) * 0.5) * gain;
        b.l[i] += v * (1 - pan * 0.5);
        b.r[i] += v * (1 + pan * 0.5);
    }
}

// Square-wave tone with a fast attack and exponential decay.
void square(Buf& b, double t0, double len, double f, double duty, double gain, double pan) {
    for (size_t i = b.at(t0); i < b.at(t0 + len); ++i) {
        const double t = static_cast<double>(i) / b.rate - t0;
        const double ph = t * f - std::floor(t * f);
        const double v = (ph < duty ? 1.0 : -1.0) * env(t, 0.002, len * 0.45) * gain;
        b.l[i] += v * (1 - pan * 0.5);
        b.r[i] += v * (1 + pan * 0.5);
    }
}

// Sample-and-hold "bit crusher": holds every Nth sample (lower sample rate).
void crush(Buf& b, double t0, double t1, int hold) {
    for (size_t i = b.at(t0); i + static_cast<size_t>(hold) < b.at(t1); i += static_cast<size_t>(hold))
        for (int k = 1; k < hold; ++k) {
            b.l[i + static_cast<size_t>(k)] = b.l[i];
            b.r[i + static_cast<size_t>(k)] = b.r[i];
        }
}

std::vector<std::int16_t> finish(Buf& b, double echo) {
    const size_t dl = static_cast<size_t>(0.19 * b.rate), dr = static_cast<size_t>(0.23 * b.rate);
    if (echo > 0)
        for (size_t i = std::max(dl, dr); i < b.l.size(); ++i) {
            b.l[i] += b.r[i - dl] * echo;
            b.r[i] += b.l[i - dr] * echo;
        }
    std::vector<std::int16_t> out(b.l.size() * 2);
    for (size_t i = 0; i < b.l.size(); ++i) {
        out[i * 2] = static_cast<std::int16_t>(std::tanh(b.l[i] * 0.9) * 30000);
        out[i * 2 + 1] = static_cast<std::int16_t>(std::tanh(b.r[i] * 0.9) * 30000);
    }
    return out;
}

}  // namespace

std::vector<std::int16_t> synthSfx(const std::string& name, int rate) {
    Buf b(rate, name == "coin" ? 1.3 : name == "ambience" ? 2.0 : 0.5);
    std::mt19937 rng(7);
    if (name == "move") {
        square(b, 0, 0.035, 1568.0, 0.25, 0.05, 0);
    } else if (name == "select") {
        square(b, 0, 0.06, 1046.5, 0.5, 0.08, 0);
        square(b, 0.06, 0.12, 1568.0, 0.5, 0.08, 0);
    } else if (name == "back") {
        square(b, 0, 0.06, 1046.5, 0.5, 0.07, 0);
        square(b, 0.06, 0.10, 698.46, 0.5, 0.07, 0);
    } else if (name == "error") {
        square(b, 0, 0.18, 110.0, 0.5, 0.10, 0);
        square(b, 0, 0.18, 116.5, 0.5, 0.08, 0);
    } else if (name == "coin") {
        // Classic coin drop: B5 then a ringing E6, then a short credit jingle.
        square(b, 0.00, 0.08, 987.77, 0.5, 0.2, 0);
        square(b, 0.08, 0.55, 1318.51, 0.5, 0.2, 0);
        hit(b, 0.0, 0.15, 0.0, rng);
        const double up[] = {659.25, 880.0, 1108.73, 1318.51, 1760.0};
        for (int k = 0; k < 5; ++k) square(b, 0.62 + k * 0.055, 0.09, up[k], 0.25, 0.09, k % 2 ? 0.3 : -0.3);
        crush(b, 0, 1.3, 3);
        return finish(b, 0.15);
    } else if (name == "ambience") {
        // Quiet attract-mode bleep for the INSERT COIN screen (played now and then).
        square(b, 0.0, 0.07, 1318.51, 0.5, 0.035, -0.3);
        square(b, 0.14, 0.07, 1760.0, 0.5, 0.035, 0.3);
        crush(b, 0, 0.4, 3);
    } else if (name == "start") {
        const double up[] = {523.25, 659.25, 783.99, 1046.5};
        for (int k = 0; k < 4; ++k) square(b, k * 0.06, 0.1, up[k], 0.5, 0.09, 0);
    }
    return finish(b, 0.0);
}

std::vector<std::int16_t> synthIntroAudio(int rate, double seconds) {
    const double s = seconds / 6.0;  // the timeline is designed for 6 s and scaled
    Buf b(rate, seconds + 0.5);
    std::mt19937 rng(1);
    std::uniform_real_distribution<double> n(-1, 1);

    // 1. Riser: filtered noise and a sweeping saw (0 .. 1.5 s).
    {
        double ph = 0, lpL = 0, lpR = 0;
        const double end = 1.5 * s;
        for (size_t i = 0; i < b.at(end); ++i) {
            const double t = static_cast<double>(i) / rate, x = t / end;
            const double f = 70 * std::pow(10.0, x * 1.1);
            ph += f / rate;
            const double saw = 2 * (ph - std::floor(ph)) - 1;
            const double cut = 0.01 + 0.25 * x * x;
            lpL += cut * (n(rng) - lpL);
            lpR += cut * (n(rng) - lpR);
            const double g = x * x * 0.35;
            b.l[i] += (saw * 0.3 + lpL * 0.7) * g;
            b.r[i] += (saw * 0.3 + lpR * 0.7) * g;
        }
    }
    // 2. One hit per logo letter (Z, E, N, 1), panned across.
    const double letters[4] = {1.45, 1.65, 1.85, 2.05};
    for (int k = 0; k < 4; ++k) hit(b, letters[k] * s, 0.55, -0.6 + 0.4 * k, rng);
    // 3. The big impact at 2.3 s: deep boom plus a noise burst.
    {
        const double t0 = 2.3 * s;
        double ph = 0, lp = 0;
        for (size_t i = b.at(t0); i < b.at(t0 + 2.5); ++i) {
            const double t = static_cast<double>(i) / rate - t0;
            ph += 2 * kPi * (38 + 60 * std::exp(-t * 9)) / rate;
            lp += 0.08 * (n(rng) - lp);
            const double v = std::sin(ph) * env(t, 0.003, 0.7) * 0.8 + lp * env(t, 0.001, 0.25) * 1.6;
            b.l[i] += v;
            b.r[i] += v;
        }
    }
    // 4. Power-up: a fast, bright square-wave arpeggio straight after the
    //    impact, like a cabinet's boot jingle (2.35 .. 2.75 s).
    {
        const double notes[] = {440.0, 554.37, 659.25, 880.0, 1108.73, 1318.51, 1760.0, 2217.46};
        for (int k = 0; k < 8; ++k)
            square(b, (2.35 + k * 0.05) * s, 0.07, notes[k], 0.25, 0.10, k % 2 ? 0.4 : -0.4);
    }
    // 5. Two gritty synth stabs (3.0 s and 3.15 s): stacked squares, crushed.
    for (double t0 : {3.0, 3.15}) {
        for (double f : {220.0, 329.63, 440.0, 659.25}) square(b, t0 * s, 0.16, f, 0.5, 0.07, 0.0);
        hit(b, t0 * s, 0.25, 0.0, rng);
    }
    // 6. The scene settles into INSERT COIN (3.4 s): a soft, wide pad that
    //    swells and fades by the end, where the menu music takes over.
    {
        const double t0 = 3.3 * s, end = seconds;
        const double notes[] = {110.0, 164.81, 220.0, 261.63, 329.63};
        for (size_t i = b.at(t0); i < b.at(end); ++i) {
            const double t = static_cast<double>(i) / rate;
            const double x = (t - t0) / (end - t0);
            const double g = 0.05 * std::sin(kPi * std::min(1.0, x * 1.6)) * (1 - x);
            double l = 0, r = 0;
            for (int k = 0; k < 5; ++k) {
                const double f = notes[k];
                l += std::sin(2 * kPi * f * 1.003 * t + k);
                r += std::sin(2 * kPi * f * 0.997 * t + 2 * k);
            }
            b.l[i] += l * g;
            b.r[i] += r * g;
        }
    }
    crush(b, 2.3 * s, 3.3 * s, 4);  // the impact and stabs get 8-bit grit; the pad stays clean
    return finish(b, 0.18);
}

bool writeWav(const std::string& path, const std::vector<std::int16_t>& stereo, int rate) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
    const auto bytes = static_cast<std::uint32_t>(stereo.size() * 2);
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32(static_cast<std::uint32_t>(rate)); u32(static_cast<std::uint32_t>(rate) * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    std::fwrite(stereo.data(), 2, stereo.size(), f);
    return std::fclose(f) == 0;
}

IntroAudioPlayer::~IntroAudioPlayer() { stop(); }

bool IntroAudioPlayer::play(double seconds, float gain) {
    stop();
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    constexpr int kRate = 48000;
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, kRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream_) return false;
    const auto pcm = synthIntroAudio(kRate, seconds);
    SDL_PutAudioStreamData(static_cast<SDL_AudioStream*>(stream_), pcm.data(), static_cast<int>(pcm.size() * 2));
    SDL_FlushAudioStream(static_cast<SDL_AudioStream*>(stream_));
    SDL_SetAudioStreamGain(static_cast<SDL_AudioStream*>(stream_), gain);
    SDL_ResumeAudioStreamDevice(static_cast<SDL_AudioStream*>(stream_));
    return true;
}

void IntroAudioPlayer::fadeOut() {
    // A short ramp would need a callback; muting then closing avoids a click.
    if (stream_) SDL_SetAudioStreamGain(static_cast<SDL_AudioStream*>(stream_), 0.0f);
    stop();
}

void IntroAudioPlayer::stop() {
    if (stream_) SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(stream_));
    stream_ = nullptr;
}

SfxPlayer::~SfxPlayer() {
    for (void* s : streams_) SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(s));
}

void SfxPlayer::play(const std::string& name, float volume) {
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return;
    // Recycle finished streams; a new "move" cuts the previous one short.
    for (auto it = streams_.begin(); it != streams_.end();) {
        auto* st = static_cast<SDL_AudioStream*>(*it);
        if (SDL_GetAudioStreamQueued(st) <= 0 || streams_.size() > 6) {
            SDL_DestroyAudioStream(st);
            it = streams_.erase(it);
        } else {
            ++it;
        }
    }
    constexpr int kRate = 48000;
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, kRate};
    SDL_AudioStream* st = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!st) return;
    const auto pcm = synthSfx(name, kRate);
    SDL_SetAudioStreamGain(st, volume);
    SDL_PutAudioStreamData(st, pcm.data(), static_cast<int>(pcm.size() * 2));
    SDL_FlushAudioStream(st);
    SDL_ResumeAudioStreamDevice(st);
    streams_.push_back(st);
}
