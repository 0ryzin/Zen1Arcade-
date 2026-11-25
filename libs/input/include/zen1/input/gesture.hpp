// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dedicated 1P / 2P cabinet button: one microswitch, three gestures.
//
//   single press   (released within 0.8 s, no second press within 0.35 s)
//                  -> Single: coin + start
//   double press   (second press within 0.35 s of the first release)
//                  -> Double, fired as the second press goes down: volume
//   hold for 5 s   -> Long: Player 1 exit / Player 2 restart
//
// A single press is only reported once the double-press window has passed,
// so a double press never inserts a coin. A press held longer than 0.8 s but
// released before 5 s does nothing (the hold was cancelled). The second
// press of a double is ignored until it is released, so holding it never
// becomes a long hold. Changes within 25 ms of the previous one are switch
// bounce and are ignored.
#pragma once

namespace zen1::input {

class ButtonGesture {
public:
    enum class Event { None, Single, Double, Long };
    struct Result {
        Event event = Event::None;
        double progress = 0.0;  // 0..1 while a possible long hold is in progress
    };

    static constexpr double kDebounce = 0.025;
    static constexpr double kShortMax = 0.8;
    static constexpr double kDoubleWindow = 0.35;
    static constexpr double kHold = 5.0;

    // Call often (every frame / every 10-20 ms) with the button state and a
    // monotonic time in seconds.
    Result update(bool down, double now);
    void reset() { state_ = State::Idle; }
    bool busy() const { return state_ != State::Idle; }

private:
    enum class State { Idle, First, WaitSecond, Second, Held };
    State state_ = State::Idle;
    bool stable_ = false;     // debounced button state
    double changed_ = -1e9;   // time of the last accepted change
    double pressAt_ = 0.0, releaseAt_ = 0.0;
};

// The cabinet's three master volume levels.
int volumeDown(int percent);  // 100 -> 50 -> 0, stays at 0
int volumeUp(int percent);    // 0 -> 50 -> 100, stays at 100
int volumePreset(int percent);  // nearest of 0 / 50 / 100
// Output gain for a preset: 0 = silent, 50 = about half as loud (-10 dB),
// 100 = full.
float volumeGain(int percent);

}  // namespace zen1::input
