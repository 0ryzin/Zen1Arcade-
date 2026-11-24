// SPDX-License-Identifier: GPL-3.0-or-later
//
// Frontend state exposed to QML as `zen1`. Phase 2: data folder, screen,
// player status, and launching the engine (test card) with a socket link.
#pragma once

#include "input_watcher.hpp"

#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QTimer>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString dataRoot READ dataRoot NOTIFY changed)
    Q_PROPERTY(QString screenInfo READ screenInfo NOTIFY changed)
    Q_PROPERTY(QString player1 READ player1 NOTIFY changed)
    Q_PROPERTY(QString player2 READ player2 NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY changed)
    Q_PROPERTY(double exitProgress READ exitProgress NOTIFY exitProgressChanged)

public:
    explicit Backend(QObject* parent = nullptr);
    ~Backend() override;

    bool init(QString* error);
    void setScreenInfo(const QString& s);

    QString version() const;
    QString dataRoot() const { return dataRoot_; }
    QString screenInfo() const { return screenInfo_; }
    QString player1() const { return input_.playerStatus(0); }
    QString player2() const { return input_.playerStatus(1); }
    QString message() const { return message_; }
    bool engineRunning() const { return engine_ != nullptr; }
    double exitProgress() const { return exitProgress_; }

    Q_INVOKABLE void launchTestCard();
    Q_INVOKABLE void refreshControllers();

signals:
    void changed();
    void exitProgressChanged();
    void quitRequested();

private:
    void setMessage(const QString& m);
    void onEngineLine(const QString& line);
    void checkExitHold();

    QString dataRoot_, controlsFile_, screenInfo_, message_;
    InputWatcher input_;
    QLocalServer server_;
    QPointer<QLocalSocket> link_;
    QProcess* engine_ = nullptr;
    QTimer holdTimer_;
    QElapsedTimer holdClock_;
    double exitProgress_ = 0.0;
};
