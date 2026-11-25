// SPDX-License-Identifier: GPL-3.0-or-later
#include "menu_music.hpp"

#include <cstddef>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace {

// "Night Signal": Zen1's menu theme, composed for Zen1 Arcade (no borrowed
// melodies, no samples). D minor, 96 BPM, 16 bars = one 40 s loop:
//   bars 1-8   pads breathe in over a pulsing bass, a plucked arpeggio
//              whose filter slowly opens, soft kick and hats, bell calls
//   bars 9-16  the lead melody, the full beat, a little more air
// Chords, two bars each: Dm9 - Bbmaj7 - Gm9 - Asus4 / A7, twice.
// Everything is rendered into a circular buffer (tails, delay and reverb
// wrap round to the start) so the loop repeats without a seam.
constexpr double kPi = 3.14159265358979323846;
constexpr double kBpm = 96.0;
constexpr int kBars = 16;
constexpr double kBeat = 60.0 / kBpm;
constexpr double kBar = 4 * kBeat;
constexpr double kLoopSeconds = kBars * kBar;  // 40 s

double hz(double midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }

// Circular stereo bus.
struct Bus {
    std::vector<double> l, r;
    explicit Bus(size_t n) : l(n, 0.0), r(n, 0.0) {}
    size_t size() const { return l.size(); }
    void add(size_t i, double v, double pan) {
        i %= l.size();
        l[i] += v * (1.0 - pan);
        r[i] += v * (1.0 + pan);
    }
};
struct Mix {
    int rate;
    Bus dry, delay, verb;  // dry, echo send, reverb send
    Mix(int rate_, size_t n) : rate(rate_), dry(n), delay(n), verb(n) {}
    void add(size_t i, double v, double pan, double toDelay, double toVerb) {
        dry.add(i, v, pan);
        if (toDelay > 0) delay.add(i, v * toDelay, pan);
        if (toVerb > 0) verb.add(i, v * toVerb, pan);
    }
};

// Band-limited saw (PolyBLEP), so the pads and bass stay smooth.
double blep(double t, double dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}
struct Osc {
    double ph = 0;
    double saw(double f, int rate) {
        const double dt = f / rate;
        double v = 2.0 * ph - 1.0 - blep(ph, dt);
        ph += dt; ph -= std::floor(ph);
        return v;
    }
    double square(double f, int rate, double duty = 0.5) {
        const double dt = f / rate;
        double v = (ph < duty ? 1.0 : -1.0) + blep(ph, dt) - blep(std::fmod(ph + 1.0 - duty, 1.0), dt);
        ph += dt; ph -= std::floor(ph);
        return v;
    }
    double sine(double f, int rate) {
        const double v = std::sin(2 * kPi * ph);
        ph += f / rate; ph -= std::floor(ph);
        return v;
    }
    double tri(double f, int rate) {
        const double v = 4.0 * std::abs(ph - 0.5) - 1.0;
        ph += f / rate; ph -= std::floor(ph);
        return v;
    }
};
// State-variable low-pass (Chamberlin), cutoff in Hz.
struct Lp {
    double low = 0, band = 0;
    double run(double x, double cutoff, double q, int rate) {
        const double f = 2.0 * std::sin(kPi * std::min(cutoff, rate * 0.2) / rate);
        low += f * band;
        const double high = x - low - q * band;
        band += f * high;
        return low;
    }
};

// Where in the loop the filter "sweep" is: closed at the start, open in the
// middle, closed again at the end, so it is continuous across the seam.
double sweep(double t) { return 0.5 - 0.5 * std::cos(2 * kPi * t / kLoopSeconds); }

// Pad note: three detuned saws through a soft low-pass; slow attack, long release.
void pad(Mix& m, double t0, double len, int midi, double gain) {
    const double rel = 1.6, att = 0.9;
    const size_t a = static_cast<size_t>(t0 * m.rate), n = static_cast<size_t>((len + rel) * m.rate);
    Osc o[3];
    o[1].ph = 0.33; o[2].ph = 0.71;
    const double det[3] = {-0.09, 0.0, 0.08};  // semitones
    Lp lpL, lpR;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        double env = std::min(1.0, t / att);
        if (t > len) env *= std::exp(-(t - len) / (rel * 0.35));
        const double cut = 700 + 900 * sweep(t0 + t) + 150 * std::sin(2 * kPi * (t0 + t) / 5.0);
        const double s0 = o[0].saw(hz(midi + det[0]), m.rate);
        const double s1 = o[1].saw(hz(midi + det[1]), m.rate);
        const double s2 = o[2].saw(hz(midi + det[2]), m.rate);
        const double vl = lpL.run(s0 * 0.6 + s1 * 0.4, cut, 0.9, m.rate) * env * gain;
        const double vr = lpR.run(s2 * 0.6 + s1 * 0.4, cut, 0.9, m.rate) * env * gain;
        m.dry.l[(a + i) % m.dry.size()] += vl;
        m.dry.r[(a + i) % m.dry.size()] += vr;
        m.verb.l[(a + i) % m.dry.size()] += vl * 0.5;
        m.verb.r[(a + i) % m.dry.size()] += vr * 0.5;
    }
}

// Bass note: saw plus a sub sine, plucky filter, ducked on every beat (the
// "breathing" of a sidechained synth bass).
void bass(Mix& m, double t0, double len, int midi, double gain) {
    const size_t a = static_cast<size_t>(t0 * m.rate), n = static_cast<size_t>(len * m.rate);
    Osc saw, sub;
    Lp lp;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        const double env = std::min(1.0, t / 0.006) * std::min(1.0, (len - t) / 0.02);
        const double beatPos = std::fmod(t0 + t, kBeat) / kBeat;
        const double duck = 0.45 + 0.55 * std::min(1.0, beatPos * 2.2);
        const double cut = 180 + 900 * std::exp(-t / 0.07) + 250 * sweep(t0 + t);
        const double v = lp.run(saw.saw(hz(midi), m.rate) * 0.7, cut, 0.7, m.rate) + sub.sine(hz(midi - 12), m.rate) * 0.55;
        m.add(a + i, v * env * duck * gain, 0.0, 0.0, 0.0);
    }
}

// Plucked arpeggio note: square + triangle, quick decay, filter following the sweep.
void pluck(Mix& m, double t0, int midi, double gain, double pan) {
    const double len = 0.42;
    const size_t a = static_cast<size_t>(t0 * m.rate), n = static_cast<size_t>(len * m.rate);
    Osc sq, tr;
    Lp lp;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        const double env = std::min(1.0, t / 0.002) * std::exp(-t / 0.11);
        const double cut = 900 + 3800 * sweep(t0) * std::exp(-t / 0.08) + 400;
        const double v = lp.run(sq.square(hz(midi), m.rate, 0.3) * 0.5 + tr.tri(hz(midi + 12), m.rate) * 0.35, cut, 1.1, m.rate);
        m.add(a + i, v * env * gain, pan, 0.55, 0.25);
    }
}

// Lead: soft square with a delayed vibrato, a short glide in, echo and reverb.
void lead(Mix& m, double t0, double len, int midi, int from, double gain) {
    const double rel = 0.25;
    const size_t a = static_cast<size_t>(t0 * m.rate), n = static_cast<size_t>((len + rel) * m.rate);
    Osc o, o2;
    Lp lp;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        double env = std::min(1.0, t / 0.03);
        if (t > len) env *= std::exp(-(t - len) / 0.07);
        const double glide = from > 0 ? (from - midi) * std::exp(-t / 0.035) : 0.0;
        const double vib = 0.12 * std::min(1.0, std::max(0.0, (t - 0.25) / 0.3)) * std::sin(2 * kPi * 5.2 * t);
        const double f = hz(midi + glide + vib);
        const double v = lp.run(o.square(f, m.rate, 0.5) * 0.55 + o2.saw(f * 1.004, m.rate) * 0.3, 2600, 1.0, m.rate);
        m.add(a + i, v * env * gain, 0.05, 0.45, 0.45);
    }
}

// Bell: two-operator FM, long ring, mostly reverb.
void bell(Mix& m, double t0, int midi, double gain, double pan) {
    const double len = 3.0;
    const size_t a = static_cast<size_t>(t0 * m.rate), n = static_cast<size_t>(len * m.rate);
    const double f = hz(midi);
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        const double idx = 2.2 * std::exp(-t / 0.4);
        const double v = std::sin(2 * kPi * f * t + idx * std::sin(2 * kPi * f * 3.5 * t));
        const double env = std::min(1.0, t / 0.003) * std::exp(-t / 0.9);
        m.add(a + i, v * env * gain, pan, 0.35, 0.8);
    }
}

void kick(Mix& m, double t0, double gain) {
    const size_t a = static_cast<size_t>(t0 * m.rate), n = static_cast<size_t>(0.35 * m.rate);
    double ph = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        const double f = 42.0 + 120.0 * std::exp(-t / 0.028);
        ph += 2 * kPi * f / m.rate;
        const double env = std::min(1.0, t / 0.002) * std::exp(-t / 0.13);
        m.add(a + i, std::sin(ph) * env * gain, 0.0, 0.0, 0.0);
    }
}

void snare(Mix& m, double t0, double gain, std::mt19937& rng) {
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    const size_t a = static_cast<size_t>(t0 * m.rate), n = static_cast<size_t>(0.45 * m.rate);
    double ph = 0, hp = 0, prev = 0;
    Lp lp;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        const double x = u(rng);
        hp = 0.85 * (hp + x - prev); prev = x;           // high-passed noise
        const double noise = lp.run(hp, 6500, 1.0, m.rate) * std::exp(-t / 0.09);
        ph += 2 * kPi * (185 + 60 * std::exp(-t / 0.02)) / m.rate;
        const double body = std::sin(ph) * std::exp(-t / 0.05);
        const double env = std::min(1.0, t / 0.001);
        m.add(a + i, (noise * 0.8 + body * 0.5) * env * gain, 0.0, 0.0, 0.9);
    }
}

void hat(Mix& m, double t0, double gain, double pan, std::mt19937& rng) {
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    const size_t a = static_cast<size_t>(t0 * m.rate), n = static_cast<size_t>(0.06 * m.rate);
    double hp = 0, prev = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / m.rate;
        const double x = u(rng);
        hp = 0.6 * (hp + x - prev); prev = x;
        m.add(a + i, hp * std::exp(-t / 0.016) * gain, pan, 0.0, 0.1);
    }
}

struct Chord {
    int bass;
    int pad[5];
    int arp[5];
};
const Chord kChords[5] = {
    {38, {50, 53, 57, 60, 64}, {62, 65, 69, 72, 76}},  // Dm9
    {34, {46, 50, 53, 57, 62}, {58, 62, 65, 69, 74}},  // Bbmaj7
    {31, {46, 50, 53, 55, 57}, {55, 58, 62, 65, 69}},  // Gm9
    {33, {50, 52, 55, 57, 62}, {57, 62, 64, 67, 69}},  // Asus4 (with the 7th)
    {33, {49, 52, 55, 57, 61}, {57, 61, 64, 67, 69}},  // A7
};
const Chord& chordAt(int bar) {
    const int c = (bar % 8) / 2;
    if (c == 3) return kChords[bar % 2 ? 4 : 3];
    return kChords[c];
}

struct Note {
    int bar;
    double beat, len;
    int midi;
};
// The lead, bars 9-16 (0-based 8..15).
const Note kLead[] = {
    {8, 0, 1.5, 69}, {8, 1.5, 0.5, 74}, {8, 2, 1, 76}, {8, 3, 1, 77},
    {9, 0, 2, 76}, {9, 2, 1, 72}, {9, 3, 1, 69},
    {10, 0, 1.5, 74}, {10, 1.5, 0.5, 77}, {10, 2, 2, 81},
    {11, 0, 1, 79}, {11, 1, 1, 77}, {11, 2, 2, 74},
    {12, 0, 1, 70}, {12, 1, 1, 74}, {12, 2, 1, 77}, {12, 3, 1, 79},
    {13, 0, 3, 81}, {13, 3, 0.5, 79}, {13, 3.5, 0.5, 77},
    {14, 0, 2, 76}, {14, 2, 1, 74}, {14, 3, 1, 76},
    {15, 0, 2, 73}, {15, 2, 1, 76}, {15, 3, 1, 69},
};

// Circular feedback delay (ping-pong), run until the wrap is settled.
void pingPong(Bus& in, Bus& out, size_t d, double fb) {
    const size_t n = in.size();
    std::vector<double> l(n, 0.0), r(n, 0.0);
    for (int pass = 0; pass < 4; ++pass)
        for (size_t i = 0; i < n; ++i) {
            const size_t j = (i + n - d) % n;
            l[i] = in.l[i] + r[j] * fb;
            r[i] = l[j] * fb + in.r[i] * 0.3;
        }
    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + n - d) % n;
        out.l[i] += r[j] * 0.8;
        out.r[i] += l[j] * 0.8;
    }
}

// Small Schroeder reverb (4 damped combs + 2 all-passes per side), run round
// the loop twice so the tail at the seam is the looped tail.
void reverb(Bus& in, Bus& out, int rate, double wet) {
    const size_t n = in.size();
    const double sc = rate / 44100.0;
    for (int side = 0; side < 2; ++side) {
        const auto& x = side ? in.r : in.l;
        auto& y = side ? out.r : out.l;
        const int combs[4] = {1557, 1617, 1491, 1422};
        const int aps[2] = {556, 441};
        std::vector<std::vector<double>> cb(4), ab(2);
        std::vector<size_t> cp(4, 0), ap(2, 0);
        std::vector<double> damp(4, 0.0);
        for (int k = 0; k < 4; ++k) cb[k].assign(static_cast<size_t>((combs[k] + side * 23) * 2.2 * sc), 0.0);
        for (int k = 0; k < 2; ++k) ab[k].assign(static_cast<size_t>((aps[k] + side * 23) * sc), 0.0);
        for (int pass = 0; pass < 2; ++pass)
            for (size_t i = 0; i < n; ++i) {
                double acc = 0;
                for (int k = 0; k < 4; ++k) {
                    double& slot = cb[k][cp[k]];
                    damp[k] = slot * 0.6 + damp[k] * 0.4;
                    acc += slot;
                    slot = x[i] * 0.25 + damp[k] * 0.86;
                    if (++cp[k] >= cb[k].size()) cp[k] = 0;
                }
                for (int k = 0; k < 2; ++k) {
                    double& slot = ab[k][ap[k]];
                    const double o = -acc + slot;
                    slot = acc + slot * 0.5;
                    acc = o;
                    if (++ap[k] >= ab[k].size()) ap[k] = 0;
                }
                if (pass == 1) y[i] += acc * wet;
            }
    }
}

}  // namespace

std::vector<std::int16_t> synthMenuMusic(int rate) {
    const size_t loop = static_cast<size_t>(std::llround(kLoopSeconds * rate));
    Mix m(rate, loop);
    std::mt19937 rng(20261125);
    const double s16 = kBeat / 4;

    for (int bar = 0; bar < kBars; ++bar) {
        const Chord& c = chordAt(bar);
        const double t = bar * kBar;
        const bool second = bar >= 8;
        // Pads: one held chord per two bars (Asus4 and A7 one bar each).
        const bool newChord = bar % 2 == 0 || (bar % 8) >= 6;
        if (newChord) {
            const double len = (bar % 8) >= 6 ? kBar : 2 * kBar;
            for (int k = 0; k < 5; ++k) pad(m, t, len, c.pad[k], 0.045);
        }
        // Bass: eighths, root with an octave lift on the last two.
        for (int e = 0; e < 8; ++e) {
            const int note = c.bass + (e >= 6 && bar % 2 ? 12 : 0);
            bass(m, t + e * kBeat / 2, kBeat / 2 * 0.9, note, second ? 0.17 : 0.13);
        }
        // Arpeggio: 16ths through the chord, panned left and right.
        static const int order[16] = {0, 2, 1, 3, 2, 4, 3, 1, 0, 2, 1, 3, 4, 3, 2, 1};
        for (int s = 0; s < 16; ++s) {
            const int note = c.arp[order[s]] + (second && s >= 12 ? 12 : 0);
            const double g = (s % 4 == 0 ? 0.05 : 0.034) * (second ? 0.8 : 1.0);
            pluck(m, t + s * s16, note, g, s % 2 ? 0.35 : -0.35);
        }
        // Drums: soft kick on 1 and 3 (and the "and" of 4 in the second half),
        // hats on the off-beats, snare on 2 and 4 only in the second half.
        kick(m, t, 0.30);
        kick(m, t + 2 * kBeat, 0.26);
        if (second && bar % 2 == 1) kick(m, t + 3.5 * kBeat, 0.18);
        if (second) {
            snare(m, t + kBeat, 0.12, rng);
            snare(m, t + 3 * kBeat, 0.12, rng);
        } else if (bar % 4 == 3) {
            snare(m, t + 3 * kBeat, 0.07, rng);
        }
        for (int e = 0; e < 8; ++e)
            hat(m, t + e * kBeat / 2 + (e % 2 ? 0.012 : 0.0), e % 2 ? 0.06 : (second ? 0.03 : 0.0), 0.3, rng);
        // Bell calls in the first half: a slow four-note question.
        if (!second && bar % 2 == 0) {
            static const int calls[4] = {81, 77, 74, 76};
            bell(m, t, calls[bar / 2], 0.05, bar % 4 ? 0.4 : -0.4);
            bell(m, t + 1.5 * kBeat, calls[bar / 2] - 5, 0.025, bar % 4 ? -0.4 : 0.4);
        }
    }
    // Lead.
    int prev = 0;
    for (const auto& n : kLead) {
        const double at = n.bar * kBar + n.beat * kBeat;
        lead(m, at, n.len * kBeat * 0.94, n.midi, prev, 0.055);
        prev = n.midi;
    }

    // Effects: dotted-eighth ping-pong echo, then reverb on everything sent.
    pingPong(m.delay, m.verb, static_cast<size_t>(0.75 * kBeat * rate), 0.42);
    {
        Bus echoOut(loop);
        pingPong(m.delay, echoOut, static_cast<size_t>(0.75 * kBeat * rate), 0.42);
        for (size_t i = 0; i < loop; ++i) { m.dry.l[i] += echoOut.l[i] * 0.55; m.dry.r[i] += echoOut.r[i] * 0.55; }
    }
    reverb(m.verb, m.dry, rate, 0.55);

    // Master: a gentle high cut (about 9 kHz) run round the loop twice, then
    // a soft limiter.
    const double k = 1.0 - std::exp(-2.0 * kPi * 9000.0 / rate);
    for (auto* ch : {&m.dry.l, &m.dry.r}) {
        double y1 = 0, y2 = 0;
        for (int pass = 0; pass < 2; ++pass)
            for (size_t i = 0; i < loop; ++i) {
                y1 += k * ((*ch)[i] - y1);
                y2 += k * (y1 - y2);
                if (pass == 1) (*ch)[i] = y2;
            }
    }
    double peak = 1e-9;
    for (size_t i = 0; i < loop; ++i) peak = std::max({peak, std::abs(m.dry.l[i]), std::abs(m.dry.r[i])});
    const double norm = 0.9 / peak;
    std::vector<std::int16_t> out(loop * 2);
    for (size_t i = 0; i < loop; ++i) {
        auto clip = [&](double v) { return static_cast<std::int16_t>(std::clamp(std::tanh(v * norm * 1.2) / std::tanh(1.2) * 20800.0, -32767.0, 32767.0)); };
        out[2 * i] = clip(m.dry.l[i]);
        out[2 * i + 1] = clip(m.dry.r[i]);
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

void MusicPlayer::prepare() {
    if (state_ || rendering_.valid()) return;
    rendering_ = std::async(std::launch::async, [] { return synthMenuMusic(kMusicRate); });
}

bool MusicPlayer::play(float gain, int fadeMs) {
    if (!stream_) {
        if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
        if (!state_) {
            prepare();
            state_ = new State;
            state_->pcm = rendering_.get();
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
