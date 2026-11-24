// SPDX-License-Identifier: GPL-3.0-or-later
#include "intro_audio.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

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

}  // namespace

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
    // 4. Neon pad: an A major chord of detuned saws, swelling then fading.
    {
        const double notes[] = {110.0, 164.81, 220.0, 277.18, 329.63, 440.0};
        const double t0 = 2.3 * s, t1 = seconds;
        double lpL = 0, lpR = 0;
        std::vector<double> ph(12, 0.0);
        for (size_t i = b.at(t0); i < b.at(t1); ++i) {
            const double t = static_cast<double>(i) / rate - t0, len = t1 - t0;
            double vl = 0, vr = 0;
            for (size_t k = 0; k < 6; ++k) {
                for (int d = 0; d < 2; ++d) {
                    auto& p = ph[k * 2 + static_cast<size_t>(d)];
                    p += notes[k] * (d ? 1.004 : 0.996) / rate;
                    const double saw = 2 * (p - std::floor(p)) - 1;
                    (d ? vr : vl) += saw;
                }
            }
            const double cut = 0.02 + 0.05 * std::min(1.0, t / 1.5);
            lpL += cut * (vl - lpL);
            lpR += cut * (vr - lpR);
            const double swell = std::min(1.0, t / 0.8) * std::min(1.0, (len - t) / (1.3 * s));
            b.l[i] += lpL * 0.06 * swell;
            b.r[i] += lpR * 0.06 * swell;
        }
    }
    // 5. Arpeggio sparkle (2.5 .. 4.7 s): bell-like sines on A major.
    {
        const double arp[] = {880.0, 1108.73, 1318.51, 1760.0, 1318.51, 1108.73};
        const double step = 0.117 * s;
        int k = 0;
        for (double t0 = 2.5 * s; t0 < 4.7 * s; t0 += step, ++k) {
            const double f = arp[k % 6];
            const double pan = (k % 2) ? 0.7 : -0.7;
            for (size_t i = b.at(t0); i < b.at(t0 + 0.5); ++i) {
                const double t = static_cast<double>(i) / rate - t0;
                const double v = (std::sin(2 * kPi * f * t) + 0.3 * std::sin(2 * kPi * f * 2.01 * t)) *
                                 env(t, 0.003, 0.12) * 0.08;
                b.l[i] += v * (1 - pan * 0.5);
                b.r[i] += v * (1 + pan * 0.5);
            }
        }
    }
    // 6. Whoosh into the library (4.8 .. 5.6 s).
    {
        double lp = 0;
        const double t0 = 4.8 * s, t1 = 5.6 * s;
        for (size_t i = b.at(t0); i < b.at(t1); ++i) {
            const double x = (static_cast<double>(i) / rate - t0) / (t1 - t0);
            lp += (0.02 + 0.3 * x) * (n(rng) - lp);
            const double g = std::sin(x * kPi) * 0.5;
            b.l[i] += lp * g * (1 - x);
            b.r[i] += lp * g * x + lp * g * 0.3;
        }
    }
    // Simple stereo echo for space, then soft-clip to 16 bit.
    const size_t dl = static_cast<size_t>(0.19 * rate), dr = static_cast<size_t>(0.23 * rate);
    for (size_t i = std::max(dl, dr); i < b.l.size(); ++i) {
        b.l[i] += b.r[i - dl] * 0.22;
        b.r[i] += b.l[i - dr] * 0.22;
    }
    std::vector<std::int16_t> out(b.l.size() * 2);
    for (size_t i = 0; i < b.l.size(); ++i) {
        out[i * 2] = static_cast<std::int16_t>(std::tanh(b.l[i] * 0.9) * 30000);
        out[i * 2 + 1] = static_cast<std::int16_t>(std::tanh(b.r[i] * 0.9) * 30000);
    }
    return out;
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

bool IntroAudioPlayer::play(double seconds) {
    stop();
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    constexpr int kRate = 48000;
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, kRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream_) return false;
    const auto pcm = synthIntroAudio(kRate, seconds);
    SDL_PutAudioStreamData(static_cast<SDL_AudioStream*>(stream_), pcm.data(), static_cast<int>(pcm.size() * 2));
    SDL_FlushAudioStream(static_cast<SDL_AudioStream*>(stream_));
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
