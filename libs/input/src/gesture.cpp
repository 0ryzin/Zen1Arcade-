// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/gesture.hpp"

#include <algorithm>

namespace zen1::input {

ButtonGesture::Result ButtonGesture::update(bool rawDown, double now) {
    Result r;
    // Debounce: accept a change only once the switch has been stable.
    bool edgeDown = false, edgeUp = false;
    if (rawDown != stable_ && now - changed_ >= kDebounce) {
        stable_ = rawDown;
        changed_ = now;
        edgeDown = rawDown;
        edgeUp = !rawDown;
    }

    switch (state_) {
    case State::Idle:
        if (edgeDown) {
            state_ = State::First;
            pressAt_ = now;
        }
        break;
    case State::First:
        if (edgeUp) {
            if (now - pressAt_ <= kShortMax) {
                state_ = State::WaitSecond;
                releaseAt_ = now;
            } else {
                state_ = State::Idle;  // hold cancelled
            }
            break;
        }
        r.progress = std::min(1.0, (now - pressAt_) / kHold);
        if (now - pressAt_ >= kHold) {
            r.event = Event::Long;
            r.progress = 1.0;
            state_ = State::Held;
        }
        break;
    case State::WaitSecond:
        if (edgeDown) {
            r.event = Event::Double;
            state_ = State::Second;
        } else if (now - releaseAt_ > kDoubleWindow) {
            r.event = Event::Single;
            state_ = State::Idle;
        }
        break;
    case State::Second:
    case State::Held:
        if (edgeUp || !stable_) state_ = State::Idle;
        break;
    }
    return r;
}

float volumeGain(int p) {
    const int v = volumePreset(p);
    return v == 0 ? 0.0f : v == 50 ? 0.316f : 1.0f;
}

int volumePreset(int p) { return p < 25 ? 0 : p < 75 ? 50 : 100; }
int volumeDown(int p) { return std::max(0, volumePreset(p) - 50); }
int volumeUp(int p) { return std::min(100, volumePreset(p) + 50); }

}  // namespace zen1::input
