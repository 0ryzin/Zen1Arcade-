// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads the Player 1 and Player 2 panels directly from Linux evdev
// (non-exclusive, so other programs still see them) and reports presses
// per player. Device-to-player mapping comes from zen1::input.
#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QString>

#include <array>
#include <memory>
#include <set>
#include <vector>

class InputWatcher : public QObject {
    Q_OBJECT
public:
    explicit InputWatcher(QObject* parent = nullptr);
    ~InputWatcher() override;

    // Re-scans devices and re-applies the saved/name-based assignment.
    void refresh(const QString& controlsFile);

    QString playerStatus(int player) const;  // 0 = P1, 1 = P2
    bool anyHeld(int player) const;

signals:
    void buttonPressed(int player, int code);
    void directionChanged(int player, int dx, int dy);
    void assignmentChanged();

private:
    struct Watched {
        int fd = -1;
        int player = 0;
        std::unique_ptr<QSocketNotifier> notifier;
        std::set<int> held;
        int dx = 0, dy = 0;
    };
    void readEvents(Watched& w);
    void closeAll();

    std::vector<std::unique_ptr<Watched>> watched_;
    std::array<QString, 2> status_;
};
