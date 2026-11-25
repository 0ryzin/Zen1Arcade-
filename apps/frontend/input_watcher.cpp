// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_watcher.hpp"

#include "zen1/input/assignment.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

using namespace zen1::input;

namespace {
const char* const kActionNames[6] = {"lp", "mp", "hp", "lk", "mk", "hk"};

bool isDeckControls(const DeviceInfo& d) {
    return (d.valve || d.virtualDevice) && d.kind == DeviceKind::Gamepad;
}
}  // namespace

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

bool InputWatcher::open(const DeviceInfo& d, std::vector<int> players, int deviceIndex, bool deck) {
    const int fd = ::open(d.eventPath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;
    auto w = std::make_unique<Watched>();
    w->fd = fd;
    w->players = std::move(players);
    w->deck = deck;
    w->deviceIndex = deviceIndex;
    w->layout = readPanelLayout(fd);
    w->key = calibrationKey(d);
    w->name = d.name;
    if (!w->players.empty() && !deck) w->state.calibration = loadCalibration(controlsFile_.toStdString(), w->key);
    // Start from a neutral state of the right size.
    w->state.apply(w->layout, EV_SYN, 0, 0);
    w->last = snapshotOf(w->layout, w->state);
    w->notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
    Watched* raw = w.get();
    connect(w->notifier.get(), &QSocketNotifier::activated, this, [this, raw] { readEvents(*raw); });
    if (deck) {
        w->cancelHold = std::make_unique<QTimer>();
        w->cancelHold->setSingleShot(true);
        w->cancelHold->setInterval(1500);
        connect(w->cancelHold.get(), &QTimer::timeout, this, [this] {
            if (capture_) emit cancelRequested();
        });
    }
    watched_.push_back(std::move(w));
    return true;
}

void InputWatcher::refresh(const QString& controlsFile) {
    controlsFile_ = controlsFile;
    closeAll();
    capture_ = false;
    devices_ = scanDevices();
    const auto a = assignPlayers(devices_, loadPlayerBindings(controlsFile.toStdString()));
    maps_ = loadButtonMaps(controlsFile.toStdString());
    device_ = {-1, -1};
    for (int p = 0; p < 2; ++p) {
        const auto& slot = a.players[static_cast<size_t>(p)];
        assigned_[static_cast<size_t>(p)] = slot.device.has_value();
        if (!slot.device) {
            status_[static_cast<size_t>(p)] = QStringLiteral("not assigned");
            continue;
        }
        device_[static_cast<size_t>(p)] = static_cast<int>(*slot.device);
        const auto& d = devices_[*slot.device];
        status_[static_cast<size_t>(p)] = QString::fromStdString(d.name) + QStringLiteral("  (") +
                                          QString::fromLatin1(toString(slot.reason)) + QStringLiteral(")");
    }
    if (!paused_) {
        if (device_[0] >= 0 && device_[0] == device_[1]) {
            open(devices_[static_cast<size_t>(device_[0])], {0, 1}, device_[0]);
        } else {
            for (int p = 0; p < 2; ++p)
                if (device_[static_cast<size_t>(p)] >= 0)
                    open(devices_[static_cast<size_t>(device_[static_cast<size_t>(p)])], {p}, device_[static_cast<size_t>(p)]);
        }
        // Steam Deck controls drive the menus while Player 1 has no panel.
        if (device_[0] < 0)
            for (size_t i = 0; i < devices_.size(); ++i)
                if (isDeckControls(devices_[i])) open(devices_[i], {0}, static_cast<int>(i), true);
    }
    emit assignmentChanged();
}

void InputWatcher::setPaused(bool paused) {
    if (paused == paused_) return;
    paused_ = paused;
    refresh(controlsFile_);
}

InputWatcher::CaptureReport InputWatcher::beginCapture(const std::string& onlyEventPath) {
    closeAll();
    capture_ = true;
    devices_ = scanDevices();
    CaptureReport r;
    for (size_t i = 0; i < devices_.size(); ++i) {
        const auto& d = devices_[i];
        const QString name = QString::fromStdString(d.name);
        // The Steam Deck's controls and Steam's virtual pads are never
        // captured. In Game Mode Steam mirrors every controller as a virtual
        // pad, so a press there is usually the same press as on the real
        // device: it must not cancel. Holding B on them for 1.5 s cancels.
        if (isDeckControls(d)) {
            open(d, {}, static_cast<int>(i), true);
            r.ignored << name;
            continue;
        }
        if (!isPlayerCandidate(d)) {
            if (d.kind == DeviceKind::Joystick || d.kind == DeviceKind::Gamepad || d.kind == DeviceKind::Keyboard)
                r.ignored << name;
            continue;
        }
        if (!onlyEventPath.empty() && d.eventPath != onlyEventPath) continue;
        if (open(d, {}, static_cast<int>(i))) r.listening << name;
        else r.unreadable << name + QStringLiteral(": ") + QString::fromLocal8Bit(std::strerror(errno));
    }
    return r;
}

void InputWatcher::endCapture() { refresh(controlsFile_); }

const DeviceInfo* InputWatcher::captureDevice(int index) const {
    return index >= 0 && static_cast<size_t>(index) < devices_.size() ? &devices_[static_cast<size_t>(index)] : nullptr;
}

Resolved InputWatcher::deckResolved(const Watched& w) const {
    // Fixed menu layout for a gamepad: A select, B back, X/Y/LB/RB the other
    // buttons, Start = 1P button, Select (View) = 2P button.
    Resolved r;
    auto btn = [&](int code) {
        const int i = w.layout.buttonIndex(code);
        return i >= 0 && static_cast<size_t>(i) < w.state.buttons.size() && w.state.buttons[static_cast<size_t>(i)];
    };
    r.dirX = w.state.dirX;
    r.dirY = w.state.dirY;
    if (btn(BTN_DPAD_LEFT)) r.dirX = -1;
    if (btn(BTN_DPAD_RIGHT)) r.dirX = 1;
    if (btn(BTN_DPAD_UP)) r.dirY = -1;
    if (btn(BTN_DPAD_DOWN)) r.dirY = 1;
    r.button = {btn(BTN_SOUTH), btn(BTN_WEST), btn(BTN_NORTH), btn(BTN_EAST), btn(BTN_TL), btn(BTN_TR)};
    r.player = btn(BTN_START);
    return r;
}

void InputWatcher::menuEvents(int player, const Resolved& before, const Resolved& now) {
    for (int a = 0; a < 6; ++a)
        if (now.button[static_cast<size_t>(a)] && !before.button[static_cast<size_t>(a)])
            emit control(player, QString::fromLatin1(kActionNames[a]));
    // A separate START or COIN button acts like the player button in menus.
    if ((now.start && !before.start) || (now.coin && !before.coin)) emit control(player, QStringLiteral("player"));
    if (suppressed_[static_cast<size_t>(player)]) return;
    if (now.dirX != before.dirX || now.dirY != before.dirY) {
        const auto p = static_cast<size_t>(player);
        dirX_[p] = now.dirX;
        dirY_[p] = now.dirY;
        if (dirX_[p] || dirY_[p]) {
            repeatTick(player);
            repeat_[p].start(380);  // first repeat after a pause, then faster
        } else {
            repeat_[p].stop();
        }
    }
}

void InputWatcher::readEvents(Watched& w) {
    input_event ev{};
    bool any = false;
    while (::read(w.fd, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev)) {
        if (!w.state.apply(w.layout, ev.type, ev.code, ev.value)) continue;
        any = true;
        const auto snap = snapshotOf(w.layout, w.state);
        if (capture_) {
            const auto b = detectInput(w.last, snap);
            if (w.deck) {
                // B (east) held starts the cancel timer; any release stops it.
                bool eastHeld = false;
                for (size_t i = 0; i < w.layout.buttonCodes.size() && i < snap.buttons.size(); ++i)
                    if (w.layout.buttonCodes[i] == BTN_EAST && snap.buttons[i]) eastHeld = true;
                if (w.cancelHold) {
                    if (eastHeld && !w.cancelHold->isActive()) w.cancelHold->start();
                    if (!eastHeld) w.cancelHold->stop();
                }
                if (b && b->kind == Bind::Kind::Button) emit virtualPress(QString::fromStdString(w.name));
            } else if (b) {
                emit captured(w.deviceIndex, QString::fromStdString(b->toString()));
            }
            w.last = snap;
            continue;
        }
        w.last = snap;
        for (int p : w.players) {
            const auto r = w.deck ? deckResolved(w) : resolve(maps_[static_cast<size_t>(p)], snap);
            menuEvents(p, w.lastResolved[static_cast<size_t>(p)], r);
            w.lastResolved[static_cast<size_t>(p)] = r;
        }
    }
    if (any && !capture_)
        for (int p : w.players) emit liveChanged(p);
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

Resolved InputWatcher::resolved(int player) const {
    Resolved out;
    for (const auto& w : watched_) {
        if (std::find(w->players.begin(), w->players.end(), player) == w->players.end()) continue;
        const auto& r = w->lastResolved[static_cast<size_t>(player)];
        out.player = out.player || r.player;
        out.volumeDown = out.volumeDown || r.volumeDown;
        out.volumeUp = out.volumeUp || r.volumeUp;
        out.exitGame = out.exitGame || r.exitGame;
        out.restartGame = out.restartGame || r.restartGame;
        out.coin = out.coin || r.coin;
        out.start = out.start || r.start;
        if (r.dirX) out.dirX = r.dirX;
        if (r.dirY) out.dirY = r.dirY;
        for (size_t i = 0; i < 6; ++i) out.button[i] = out.button[i] || r.button[i];
    }
    // The Deck's Select (View) button is the 2P button while Player 2 has no panel.
    if (player == 1 && !assigned_[1]) {
        for (const auto& w : watched_) {
            if (!w->deck) continue;
            const int i = w->layout.buttonIndex(BTN_SELECT);
            if (i >= 0 && static_cast<size_t>(i) < w->state.buttons.size() && w->state.buttons[static_cast<size_t>(i)])
                out.player = true;
        }
    }
    return out;
}

InputWatcher::Live InputWatcher::live(int player) const {
    Live l;
    for (const auto& w : watched_) {
        if (std::find(w->players.begin(), w->players.end(), player) == w->players.end()) continue;
        if (l.connected && w->deck) continue;  // a panel wins over the Deck
        l.connected = true;
        const auto& r = w->lastResolved[static_cast<size_t>(player)];
        l.dirX = r.dirX;
        l.dirY = r.dirY;
        l.nx = w->state.nx;
        l.ny = w->state.ny;
        l.hasAxes = w->layout.hasXY;
        l.hasHat = w->layout.hasHat;
        l.buttons = w->state.buttons;
        l.buttons.resize(std::max<size_t>(l.buttons.size(), w->layout.buttonCodes.size()), false);
        l.axes = w->state.axes;
        l.hats = w->state.hats;
        l.calibrationKey = w->key;
        l.calibration = w->state.calibration;
        l.name = w->name;
        l.source = w->deck ? "STEAM DECK" : "PANEL";
    }
    return l;
}

void InputWatcher::setStickSuppressed(int player, bool on) {
    suppressed_[static_cast<size_t>(player == 1 ? 1 : 0)] = on;
    repeat_[static_cast<size_t>(player == 1 ? 1 : 0)].stop();
}

void InputWatcher::setCalibration(int player, const StickCalibration& c) {
    for (auto& w : watched_)
        if (!w->deck && std::find(w->players.begin(), w->players.end(), player) != w->players.end())
            w->state.calibration = c;
}
