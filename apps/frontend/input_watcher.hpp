// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads the Player 1 and Player 2 panels directly from Linux evdev
// (non-exclusive, so the engine and other programs still see them) and turns
// them into menu controls: up/down/left/right (with auto-repeat), lp mp hp
// lk mk hk and player, using the same button numbers and maps as the engine.
//
// Capture mode listens to every possible panel (not only the assigned ones)
// so the controller screen can assign or remap by "press a button".
#pragma once

#include "zen1/input/bindings.hpp"
#include "zen1/input/devices.hpp"
#include "zen1/input/evdev_panel.hpp"

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
    bool playerButtonHeld(int player) const;
    bool anyHeld(int player) const;

    // Live state for the test and calibration screens.
    struct Live {
        bool connected = false;
        int dirX = 0, dirY = 0;
        double nx = 0, ny = 0;          // raw stick position, -1..1 (axes only)
        bool hasAxes = false, hasHat = false;
        std::vector<bool> buttons;
        std::string calibrationKey;
        zen1::input::StickCalibration calibration;
        std::string name;
    };
    Live live(int player) const;
    // While calibrating, a player's stick does not move the menus.
    void setStickSuppressed(int player, bool on);
    void setCalibration(int player, const zen1::input::StickCalibration& c);
    zen1::input::PanelButtonMap buttonMap(int player) const { return maps_[static_cast<size_t>(player == 1 ? 1 : 0)]; }

    // Capture: every candidate panel reports its raw button presses.
    void beginCapture();
    void endCapture();
    const zen1::input::DeviceInfo* captureDevice(int index) const;

signals:
    void control(int player, const QString& name);
    void rawButton(int player, int buttonIndex);       // assigned panels
    void captured(int deviceIndex, int buttonIndex);   // capture mode
    void assignmentChanged();
    void liveChanged(int player);

private:
    struct Watched {
        int fd = -1;
        int player = -1;       // -1 in capture mode
        int deviceIndex = -1;
        std::string key, name;
        zen1::input::PanelLayout layout;
        zen1::input::PanelEventState state;
        std::unique_ptr<QSocketNotifier> notifier;
    };
    void open(const zen1::input::DeviceInfo& d, int player, int deviceIndex);
    void readEvents(Watched& w);
    void closeAll();
    void repeatTick(int player);

    QString controlsFile_;
    std::vector<std::unique_ptr<Watched>> watched_;
    std::vector<zen1::input::DeviceInfo> devices_;
    std::array<QString, 2> status_;
    std::array<bool, 2> assigned_{};
    std::array<zen1::input::PanelButtonMap, 2> maps_{};
    std::array<QTimer, 2> repeat_;
    std::array<int, 2> dirX_{}, dirY_{};
    bool capture_ = false;
    std::array<bool, 2> suppressed_{};
    bool paused_ = false;
};
