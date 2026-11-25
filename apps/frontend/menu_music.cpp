// SPDX-License-Identifier: GPL-3.0-or-later
#include "menu_music.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kBpm = 120.0;
constexpr int kBars = 16;
constexpr double kBeat = 60.0 / kBpm;
constexpr double kLoopSeconds = kBars * 4 * kBeat;  // 32 s

double hz(int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }

struct Mix {
    int rate;
    std::vector<double> l, r;
    void add(size_t i, double v, double pan) {
        if (i >= l.size()) return;
        l[i] += v * (1.0 - pan) * 0.5 * 2;
        r[i] += v * (1.0 + pan) * 0.5 * 2;
    }
};

// Pulse wave with a soft attack/decay so notes never click.
void pulse(Mix& m, double t0, double len, int midi, double duty, double gain, double pan, double decay = 0.0) {
    const double f = hz(midi);
    const size_t a = static_cast<size_t>(t0 * m.rate);
    const size_t n = static_cast<size_t>(len * m.rate);
    double ph = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        double env = std::min(1.0, t / 0.004) * std::min(1.0, (len - t) / 0.012);
        if (decay > 0) env *= std::exp(-t / decay);
        ph += f / m.rate;
        ph -= std::floor(ph);
        const double v = (ph < duty ? 1.0 : -1.0);
        m.add(a + i, v * env * gain, pan);
    }
}

// Triangle (the classic chip bass/lead colour).
void tri(Mix& m, double t0, double len, int midi, double gain, double pan) {
    const double f = hz(midi);
    const size_t a = static_cast<size_t>(t0 * m.rate);
    const size_t n = static_cast<size_t>(len * m.rate);
    double ph = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        const double env = std::min(1.0, t / 0.003) * std::min(1.0, (len - t) / 0.01);
        ph += f / m.rate;
        ph -= std::floor(ph);
        const double v = 4.0 * std::abs(ph - 0.5) - 1.0;
        m.add(a + i, v * env * gain, pan);
    }
}

void noise(Mix& m, double t0, double len, double gain, double pan, std::mt19937& rng, int hold) {
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    const size_t a = static_cast<size_t>(t0 * m.rate);
    const size_t n = static_cast<size_t>(len * m.rate);
    double v = 0;
    for (size_t i = 0; i < n; ++i) {
        if (i % static_cast<size_t>(hold) == 0) v = u(rng);  // sample-and-hold = chip noise
        const double t = static_cast<double>(i) / m.rate;
        const double env = std::min(1.0, t / 0.001) * std::exp(-t / (len * 0.3));
        m.add(a + i, v * env * gain, pan);
    }
}

void kick(Mix& m, double t0, double gain) {
    const size_t a = static_cast<size_t>(t0 * m.rate);
    const size_t n = static_cast<size_t>(0.18 * m.rate);
    double ph = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        const double f = 45.0 + 110.0 * std::exp(-t / 0.03);
        ph += 2 * kPi * f / m.rate;
        const double env = std::min(1.0, t / 0.002) * std::exp(-t / 0.06);
        m.add(a + i, std::sin(ph) * env * gain, 0.0);
    }
}

struct Chord {
    int root;            // bass note (MIDI)
    int tones[3];        // arpeggio notes
};
// i - VI - III - VII - i - VI - VII - V (A minor), twice.
const Chord kChords[8] = {
    {45, {69, 72, 76}},  // Am
    {41, {65, 69, 72}},  // F
    {48, {67, 72, 76}},  // C
    {43, {67, 71, 74}},  // G
    {45, {69, 72, 76}},  // Am
    {41, {65, 69, 72}},  // F
    {43, {67, 71, 74}},  // G
    {40, {68, 71, 76}},  // E (major: leads back to Am)
};

struct Note {
    int bar;
    double beat, len;
    int midi;
};
// The lead, bars 9-16 (0-based 8..15).
const Note kLead[] = {
    {8, 0, 1, 76}, {8, 1, 0.5, 81}, {8, 1.5, 0.5, 79}, {8, 2, 1, 76}, {8, 3, 0.5, 72}, {8, 3.5, 0.5, 74},
    {9, 0, 1.5, 77}, {9, 1.5, 0.5, 76}, {9, 2, 1, 72}, {9, 3, 1, 69},
    {10, 0, 1, 79}, {10, 1, 0.5, 76}, {10, 1.5, 0.5, 79}, {10, 2, 1.5, 84}, {10, 3.5, 0.5, 83},
    {11, 0, 0.5, 81}, {11, 0.5, 0.5, 79}, {11, 1, 1, 74}, {11, 2, 1, 71}, {11, 3, 1, 74},
    {12, 0, 0.5, 76}, {12, 0.5, 0.5, 76}, {12, 1, 1, 81}, {12, 2, 0.5, 83}, {12, 2.5, 0.5, 84}, {12, 3, 0.5, 83}, {12, 3.5, 0.5, 81},
    {13, 0, 1, 84}, {13, 1, 1, 81}, {13, 2, 1, 77}, {13, 3, 1, 81},
    {14, 0, 1, 83}, {14, 1, 0.5, 79}, {14, 1.5, 0.5, 74}, {14, 2, 1, 79}, {14, 3, 1, 83},
    {15, 0, 1.5, 80}, {15, 1.5, 0.5, 83}, {15, 2, 1, 88}, {15, 3, 0.5, 86}, {15, 3.5, 0.5, 83},
};

}  // namespace

std::vector<std::int16_t> synthMenuMusic(int rate) {
    const size_t loop = static_cast<size_t>(std::llround(kLoopSeconds * rate));
    const size_t tail = static_cast<size_t>(2.0 * rate);
    Mix m{rate, std::vector<double>(loop + tail, 0.0), std::vector<double>(loop + tail, 0.0)};
    std::mt19937 rng(20261125);
    const double s16 = kBeat / 4;

    for (int bar = 0; bar < kBars; ++bar) {
        const Chord& c = kChords[bar % 8];
        const double t = bar * 4 * kBeat;
        // Bass: root / octave eighths, 25 % pulse.
        for (int e = 0; e < 8; ++e) {
            const int note = c.root + ((e % 2) ? 12 : 0);
            pulse(m, t + e * kBeat / 2, kBeat / 2 * 0.85, note, 0.25, 0.10, -0.1);
        }
        // Arpeggio: 16ths up and down the chord, 12.5 % pulse, with a 3/16 echo.
        static const int order[8] = {0, 1, 2, 1, 0, 1, 2, 1};
        for (int s = 0; s < 16; ++s) {
            const int note = c.tones[order[s % 8]] + (s >= 8 && bar % 2 ? 12 : 0);
            const double at = t + s * s16;
            const double g = bar < 8 ? 0.050 : 0.036;
            pulse(m, at, s16 * 0.9, note, 0.125, g, 0.35, 0.09);
            pulse(m, at + 3 * s16, s16 * 0.9, note, 0.125, g * 0.4, -0.45, 0.09);
        }
        // Drums: kick on 1 and 3, noise snare on 2 and 4, closed hats on eighths.
        for (int b = 0; b < 4; ++b) {
            if (b % 2 == 0) kick(m, t + b * kBeat, 0.22);
            else {
                noise(m, t + b * kBeat, 0.16, 0.07, 0.0, rng, 3);
                tri(m, t + b * kBeat, 0.05, 50, 0.06, 0.0);
            }
        }
        for (int e = 0; e < 8; ++e) noise(m, t + e * kBeat / 2, 0.035, e % 2 ? 0.025 : 0.016, 0.25, rng, 1);
        // Bleeps: a little coin-op chatter at the end of every other bar.
        if (bar % 2 == 1) {
            std::uniform_int_distribution<int> pick(0, 2);
            const int base = c.tones[pick(rng)] + 24;
            pulse(m, t + 3.5 * kBeat, 0.05, base, 0.5, 0.020, 0.6);
            pulse(m, t + 3.625 * kBeat, 0.05, base + 7, 0.5, 0.020, 0.6);
        }
        // Rising "power-up" sweep into the second half.
        if (bar == 7)
            for (int k = 0; k < 16; ++k) pulse(m, t + 3 * kBeat + k * kBeat / 16, kBeat / 16, 60 + k * 2, 0.5, 0.018, -0.5);
    }
    // Lead: 50 % pulse with a little vibrato feel from a detuned triangle under it.
    for (const auto& n : kLead) {
        const double at = n.bar * 4 * kBeat + n.beat * kBeat;
        pulse(m, at, n.len * kBeat * 0.92, n.midi, 0.5, 0.045, 0.0);
        tri(m, at, n.len * kBeat * 0.92, n.midi - 12, 0.05, 0.0);
    }

    // Wrap the tails (echo, decays) round to the start: seamless loop.
    for (size_t i = 0; i < tail; ++i) {
        m.l[i] += m.l[loop + i];
        m.r[i] += m.r[loop + i];
    }
    // Gentle low-pass (about 6 kHz, two poles) takes the fizz off the
    // naive pulse waves, like a cabinet speaker would. Run over the loop
    // twice so the filter state at the seam is the looped state.
    const double k = 1.0 - std::exp(-2.0 * kPi * 6000.0 / rate);
    for (auto* ch : {&m.l, &m.r}) {
        double y1 = 0, y2 = 0;
        for (int pass = 0; pass < 2; ++pass)
            for (size_t i = 0; i < loop; ++i) {
                y1 += k * ((*ch)[i] - y1);
                y2 += k * (y1 - y2);
                if (pass == 1) (*ch)[i] = y2;
            }
    }
    std::vector<std::int16_t> out(loop * 2);
    for (size_t i = 0; i < loop; ++i) {
        auto clip = [](double v) { return static_cast<std::int16_t>(std::clamp(std::tanh(v * 1.8) * 25000.0, -32767.0, 32767.0)); };
        out[2 * i] = clip(m.l[i]);
        out[2 * i + 1] = clip(m.r[i]);
    }
    return out;
}

// ------------------------------------------------------------------ player

namespace {
constexpr int kMusicRate = 44100;

void SDLCALL feed(void* user, SDL_AudioStream* stream, int additional, int) {
    auto* st = static_cast<MusicPlayer::State*>(user);
    if (st->pcm.empty() || additional <= 0) return;
    std::vector<std::int16_t> buf(static_cast<size_t>(additional / 2 + 2) & ~size_t{1});
    for (size_t i = 0; i < buf.size(); i += 2) {
        if (st->gain != st->target) {
            st->gain += st->step;
            if ((st->step > 0 && st->gain > st->target) || (st->step < 0 && st->gain < st->target)) st->gain = st->target;
        }
        buf[i] = static_cast<std::int16_t>(st->pcm[st->pos] * st->gain);
        buf[i + 1] = static_cast<std::int16_t>(st->pcm[st->pos + 1] * st->gain);
        st->pos += 2;
        if (st->pos >= st->pcm.size()) st->pos = 0;
    }
    SDL_PutAudioStreamData(stream, buf.data(), static_cast<int>(buf.size() * sizeof(std::int16_t)));
}
}  // namespace

MusicPlayer::~MusicPlayer() {
    if (stream_) SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(stream_));
    delete state_;
}

bool MusicPlayer::play(float gain, int fadeMs) {
    if (!stream_) {
        if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
        if (!state_) {
            state_ = new State;
            state_->pcm = synthMenuMusic(kMusicRate);
        }
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, kMusicRate};
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, state_);
        if (!stream_) return false;
    }
    setGain(gain, fadeMs);
    SDL_ResumeAudioStreamDevice(static_cast<SDL_AudioStream*>(stream_));
    return true;
}

void MusicPlayer::setGain(float gain, int fadeMs) {
    if (!stream_ || !state_) return;
    auto* s = static_cast<SDL_AudioStream*>(stream_);
    SDL_LockAudioStream(s);
    state_->target = std::clamp(gain, 0.0f, 1.0f);
    const float frames = std::max(1.0f, static_cast<float>(fadeMs) * kMusicRate / 1000.0f);
    state_->step = (state_->target - state_->gain) / frames;
    if (state_->step == 0.0f) state_->gain = state_->target;
    SDL_UnlockAudioStream(s);
}

void MusicPlayer::fadeOut(int fadeMs) { setGain(0.0f, fadeMs); }

bool MusicPlayer::playing() const { return stream_ && state_ && state_->target > 0.0f; }
