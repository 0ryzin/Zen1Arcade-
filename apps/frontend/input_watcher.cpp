// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_watcher.hpp"

#include "zen1/input/assignment.hpp"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

namespace {
const char* const kActionNames[6] = {"lp", "mp", "hp", "lk", "mk", "hk"};
}

InputWatcher::InputWatcher(QObject* parent) : QObject(parent) {
    status_ = {QStringLiteral("not assigned"), QStringLiteral("not assigned")};
    for (int p = 0; p < 2; ++p) {
        repeat_[static_cast<size_t>(p)].setSingleShot(false);
        connect(&repeat_[static_cast<size_t>(p)], &QTimer::timeout, this, [this, p] { repeatTick(p); });
    }
}

InputWatcher::~InputWatcher() { closeAll(); }

void InputWatcher::closeAll() {
    for (auto& w : watched_) {
        w->notifier.reset();
        if (w->fd >= 0) ::close(w->fd);
    }
    watched_.clear();
    for (auto& t : repeat_) t.stop();
    dirX_ = {};
    dirY_ = {};
}

void InputWatcher::open(const zen1::input::DeviceInfo& d, int player, int deviceIndex) {
    const int fd = ::open(d.eventPath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return;
    auto w = std::make_unique<Watched>();
    w->fd = fd;
    w->player = player;
    w->deviceIndex = deviceIndex;
    w->layout = zen1::input::readPanelLayout(fd);
    w->notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
    Watched* raw = w.get();
    connect(w->notifier.get(), &QSocketNotifier::activated, this, [this, raw] { readEvents(*raw); });
    watched_.push_back(std::move(w));
}

void InputWatcher::refresh(const QString& controlsFile) {
    controlsFile_ = controlsFile;
    closeAll();
    capture_ = false;
    devices_ = zen1::input::scanDevices();
    const auto a = zen1::input::assignPlayers(devices_, zen1::input::loadPlayerBindings(controlsFile.toStdString()));
    maps_ = zen1::input::loadButtonMaps(controlsFile.toStdString());
    for (int p = 0; p < 2; ++p) {
        const auto& slot = a.players[static_cast<size_t>(p)];
        assigned_[static_cast<size_t>(p)] = slot.device.has_value();
        if (!slot.device) {
            status_[static_cast<size_t>(p)] = QStringLiteral("not assigned");
            continue;
        }
        const auto& d = devices_[*slot.device];
        status_[static_cast<size_t>(p)] = QString::fromStdString(d.name) + QStringLiteral("  (") +
                                          QString::fromLatin1(zen1::input::toString(slot.reason)) +
                                          QStringLiteral(")");
        if (!paused_) open(d, p, static_cast<int>(*slot.device));
    }
    emit assignmentChanged();
}

void InputWatcher::setPaused(bool paused) {
    if (paused == paused_) return;
    paused_ = paused;
    refresh(controlsFile_);
}

void InputWatcher::beginCapture() {
    closeAll();
    capture_ = true;
    devices_ = zen1::input::scanDevices();
    for (size_t i = 0; i < devices_.size(); ++i)
        if (zen1::input::isPlayerCandidate(devices_[i])) open(devices_[i], -1, static_cast<int>(i));
}

void InputWatcher::endCapture() { refresh(controlsFile_); }

const zen1::input::DeviceInfo* InputWatcher::captureDevice(int index) const {
    return index >= 0 && static_cast<size_t>(index) < devices_.size() ? &devices_[static_cast<size_t>(index)]
                                                                       : nullptr;
}

void InputWatcher::readEvents(Watched& w) {
    input_event ev{};
    while (::read(w.fd, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev)) {
        const auto before = w.state.buttons;
        const int oldX = w.state.dirX, oldY = w.state.dirY;
        if (!w.state.apply(w.layout, ev.type, ev.code, ev.value)) continue;

        // Newly pressed buttons.
        for (size_t i = 0; i < w.state.buttons.size(); ++i) {
            if (!w.state.buttons[i] || (i < before.size() && before[i])) continue;
            const int idx = static_cast<int>(i);
            if (capture_) {
                emit captured(w.deviceIndex, idx);
                continue;
            }
            emit rawButton(w.player, idx);
            const auto& m = maps_[static_cast<size_t>(w.player)];
            if (idx == m.player) emit control(w.player, QStringLiteral("player"));
            for (int a = 0; a < 6; ++a)
                if (idx == m.action[static_cast<size_t>(a)]) emit control(w.player, QString::fromLatin1(kActionNames[a]));
        }
        if (capture_ || w.player < 0) continue;
        if (w.state.dirX != oldX || w.state.dirY != oldY) {
            const auto p = static_cast<size_t>(w.player);
            dirX_[p] = w.state.dirX;
            dirY_[p] = w.state.dirY;
            if (dirX_[p] || dirY_[p]) {
                repeatTick(w.player);
                repeat_[p].start(380);  // first repeat after a pause, then faster
            } else {
                repeat_[p].stop();
            }
        }
    }
}

void InputWatcher::repeatTick(int player) {
    const auto p = static_cast<size_t>(player);
    if (dirY_[p] < 0) emit control(player, QStringLiteral("up"));
    else if (dirY_[p] > 0) emit control(player, QStringLiteral("down"));
    if (dirX_[p] < 0) emit control(player, QStringLiteral("left"));
    else if (dirX_[p] > 0) emit control(player, QStringLiteral("right"));
    if (repeat_[p].isActive() && repeat_[p].interval() != 110) repeat_[p].setInterval(110);
}

QString InputWatcher::playerStatus(int player) const { return status_[static_cast<size_t>(player == 1 ? 1 : 0)]; }
bool InputWatcher::assigned(int player) const { return assigned_[static_cast<size_t>(player == 1 ? 1 : 0)]; }

bool InputWatcher::playerButtonHeld(int player) const {
    for (const auto& w : watched_) {
        if (w->player != player) continue;
        const int idx = maps_[static_cast<size_t>(player)].player;
        if (idx >= 0 && static_cast<size_t>(idx) < w->state.buttons.size() && w->state.buttons[static_cast<size_t>(idx)])
            return true;
    }
    return false;
}

bool InputWatcher::anyHeld(int player) const {
    for (const auto& w : watched_)
        if (w->player == player)
            for (bool b : w->state.buttons)
                if (b) return true;
    return false;
}
