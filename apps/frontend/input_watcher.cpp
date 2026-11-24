// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_watcher.hpp"

#include "zen1/input/assignment.hpp"
#include "zen1/input/bindings.hpp"
#include "zen1/input/devices.hpp"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

InputWatcher::InputWatcher(QObject* parent) : QObject(parent) {
    status_ = {QStringLiteral("not assigned"), QStringLiteral("not assigned")};
}

InputWatcher::~InputWatcher() { closeAll(); }

void InputWatcher::closeAll() {
    for (auto& w : watched_) {
        w->notifier.reset();
        if (w->fd >= 0) ::close(w->fd);
    }
    watched_.clear();
}

void InputWatcher::refresh(const QString& controlsFile) {
    closeAll();
    const auto devices = zen1::input::scanDevices();
    const auto saved = zen1::input::loadPlayerBindings(controlsFile.toStdString());
    const auto a = zen1::input::assignPlayers(devices, saved);

    for (int p = 0; p < 2; ++p) {
        const auto& slot = a.players[static_cast<size_t>(p)];
        if (!slot.device) {
            status_[static_cast<size_t>(p)] = QStringLiteral("not assigned");
            continue;
        }
        const auto& d = devices[*slot.device];
        status_[static_cast<size_t>(p)] =
            QString::fromStdString(d.name) + QStringLiteral("  (") +
            QString::fromLatin1(zen1::input::toString(slot.reason)) + QStringLiteral(")");

        const int fd = ::open(d.eventPath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        auto w = std::make_unique<Watched>();
        w->fd = fd;
        w->player = p;
        w->notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
        Watched* raw = w.get();
        connect(w->notifier.get(), &QSocketNotifier::activated, this, [this, raw] { readEvents(*raw); });
        watched_.push_back(std::move(w));
    }
    emit assignmentChanged();
}

void InputWatcher::readEvents(Watched& w) {
    input_event ev{};
    while (::read(w.fd, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev)) {
        if (ev.type == EV_KEY) {
            if (ev.value == 1) {
                w.held.insert(ev.code);
                emit buttonPressed(w.player, ev.code);
            } else if (ev.value == 0) {
                w.held.erase(ev.code);
            }
        } else if (ev.type == EV_ABS) {
            int dx = w.dx, dy = w.dy;
            if (ev.code == ABS_HAT0X) dx = ev.value < 0 ? -1 : ev.value > 0 ? 1 : 0;
            if (ev.code == ABS_HAT0Y) dy = ev.value < 0 ? -1 : ev.value > 0 ? 1 : 0;
            // Digital sticks on generic encoders often report 0..255 axes;
            // a proper per-device calibration arrives in Phase 3.
            if (ev.code == ABS_X) dx = ev.value < 64 ? -1 : ev.value > 192 ? 1 : 0;
            if (ev.code == ABS_Y) dy = ev.value < 64 ? -1 : ev.value > 192 ? 1 : 0;
            if (dx != w.dx || dy != w.dy) {
                w.dx = dx;
                w.dy = dy;
                emit directionChanged(w.player, dx, dy);
            }
        }
    }
}

QString InputWatcher::playerStatus(int player) const {
    return status_[static_cast<size_t>(player == 1 ? 1 : 0)];
}

bool InputWatcher::anyHeld(int player) const {
    for (const auto& w : watched_)
        if (w->player == player && !w->held.empty()) return true;
    return false;
}
