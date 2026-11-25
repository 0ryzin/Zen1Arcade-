// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads the Player 1 and Player 2 panels directly from Linux evdev
// (non-exclusive, so the engine and other programs still see them) and turns
// them into menu controls: up/down/left/right (with auto-repeat), lp mp hp
// lk mk hk, using the same numbering and maps (zen1/input/mapping.hpp) as the
// engine. The dedicated player button and the system buttons are not menu
// events: the backend reads their state and runs the gestures itself.
//
// The Steam Deck's own controls (Valve or Steam virtual gamepads) can drive
// the menus as Player 1 while no Player 1 panel is assigned. They are never
// assigned as a cabinet player.
//
// Capture mode listens to every possible panel (not only the assigned ones)
// and reports the first new input (button, hat or axis) of each.
#pragma once

#include "zen1/input/bindings.hpp"
#include "zen1/input/devices.hpp"
#include "zen1/input/evdev_panel.hpp"
#include "zen1/input/mapping.hpp"

#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <QTimer>

#include <array>
#include <memory>
#include <vector>

class InputWatcher : public QObject {
    Q_OBJECT
public:
    explicit InputWatcher(QObject* parent = nullptr);
    ~InputWatcher() override;

    // Re-scans devices and re-applies the saved/name-based assignment.
    void refresh(const QString& controlsFile);
    void setPaused(bool paused);  // while a game runs the engine owns the panels

    QString playerStatus(int player) const;  // 0 = P1, 1 = P2
    bool assigned(int player) const;
    int assignedDevice(int player) const { return device_[static_cast<size_t>(player == 1 ? 1 : 0)]; }
    const std::vector<zen1::input::DeviceInfo>& devices() const { return devices_; }

    // Mapped state of one player (all sources) for gestures and the test page.
    zen1::input::Resolved resolved(int player) const;
    bool playerButtonHeld(int player) const { return resolved(player).player; }

    // Live state for the test and calibration screens.
    struct Live {
        bool connected = false;
        int dirX = 0, dirY = 0;
        double nx = 0, ny = 0;          // raw stick position, -1..1 (axes only)
        bool hasAxes = false, hasHat = false;
        std::vector<bool> buttons;
        std::vector<double> axes;
        std::vector<std::array<int, 2>> hats;
        std::string calibrationKey;
        zen1::input::StickCalibration calibration;
        std::string name;
        std::string source;             // "PANEL" or "STEAM DECK"
    };
    Live live(int player) const;
    void setStickSuppressed(int player, bool on);
    void setCalibration(int player, const zen1::input::StickCalibration& c);
    const zen1::input::PanelButtonMap& buttonMap(int player) const { return maps_[static_cast<size_t>(player == 1 ? 1 : 0)]; }
    void setButtonMaps(const std::array<zen1::input::PanelButtonMap, 2>& m) { maps_ = m; }

    // Capture: every candidate panel reports its first new input.
    void beginCapture(const std::string& onlyEventPath = {});
    void endCapture();
    bool capturing() const { return capture_; }
    const zen1::input::DeviceInfo* captureDevice(int index) const;

signals:
    void control(int player, const QString& name);
    void captured(int deviceIndex, const QString& bind);  // capture mode: zen1::input::Bind text
    void cancelRequested();                               // capture mode: a Steam Deck button
    void assignmentChanged();
    void liveChanged(int player);

private:
    struct Watched {
        int fd = -1;
        std::vector<int> players;  // players this board serves (2 for a shared board); empty in capture mode
        bool deck = false;         // Steam Deck controls: menus only, as Player 1
        int deviceIndex = -1;
        std::string key, name;
        zen1::input::PanelLayout layout;
        zen1::input::PanelEventState state;
        zen1::input::PanelSnapshot last;
        std::array<zen1::input::Resolved, 2> lastResolved{};
        std::unique_ptr<QSocketNotifier> notifier;
    };
    void open(const zen1::input::DeviceInfo& d, std::vector<int> players, int deviceIndex, bool deck = false);
    void readEvents(Watched& w);
    void closeAll();
    void repeatTick(int player);
    zen1::input::Resolved deckResolved(const Watched& w) const;
    void menuEvents(int player, const zen1::input::Resolved& before, const zen1::input::Resolved& now);

    QString controlsFile_;
    std::vector<std::unique_ptr<Watched>> watched_;
    std::vector<zen1::input::DeviceInfo> devices_;
    std::array<QString, 2> status_;
    std::array<bool, 2> assigned_{};
    std::array<int, 2> device_{-1, -1};
    std::array<zen1::input::PanelButtonMap, 2> maps_{};
    std::array<QTimer, 2> repeat_;
    std::array<int, 2> dirX_{}, dirY_{};
    bool capture_ = false;
    std::array<bool, 2> suppressed_{};
    bool paused_ = false;
};
