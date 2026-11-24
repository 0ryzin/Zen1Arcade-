// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend.hpp"

#include "zen1/data_root.hpp"
#include "zen1/log.hpp"
#include "zen1/version.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

Backend::Backend(QObject* parent) : QObject(parent) {
    connect(&input_, &InputWatcher::assignmentChanged, this, &Backend::changed);
    connect(&input_, &InputWatcher::buttonPressed, this, [this](int player, int) {
        // Phase 2: any Player 1 button opens the test card.
        if (player == 0 && !engine_) launchTestCard();
    });
    holdTimer_.setInterval(50);
    connect(&holdTimer_, &QTimer::timeout, this, &Backend::checkExitHold);
    holdTimer_.start();
}

Backend::~Backend() {
    if (engine_) {
        engine_->terminate();
        engine_->waitForFinished(2000);
    }
}

QString Backend::version() const { return QString::fromLatin1(zen1::kVersion); }

bool Backend::init(QString* error) {
    const zen1::DataPaths paths{zen1::defaultDataRoot()};
    const auto r = zen1::ensureDataRoot(paths.root);
    dataRoot_ = QString::fromStdString(paths.root.string());
    controlsFile_ = QString::fromStdString(paths.controlsFile().string());
    if (!r.ok) {
        if (error) *error = QString::fromStdString(r.error);
        return false;
    }
    zen1::log::openSessionLog(paths.logs(), "frontend");
    zen1::log::info(std::string("zen1-arcade ") + zen1::kVersion + " data folder " + paths.root.string());

    // Socket for the engine link, private to this user.
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) runtime = QDir::tempPath();
    const QString sock = runtime + QStringLiteral("/zen1-arcade-%1.sock").arg(QCoreApplication::applicationPid());
    QLocalServer::removeServer(sock);
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_.listen(sock)) zen1::log::warn("engine link unavailable: " + server_.errorString().toStdString());
    connect(&server_, &QLocalServer::newConnection, this, [this] {
        link_ = server_.nextPendingConnection();
        if (!link_) return;
        connect(link_, &QLocalSocket::readyRead, this, [this] {
            while (link_ && link_->canReadLine())
                onEngineLine(QString::fromUtf8(link_->readLine()).trimmed());
        });
    });

    input_.refresh(controlsFile_);
    emit changed();
    return true;
}

void Backend::setScreenInfo(const QString& s) {
    screenInfo_ = s;
    emit changed();
}

void Backend::setMessage(const QString& m) {
    message_ = m;
    emit changed();
}

void Backend::refreshControllers() { input_.refresh(controlsFile_); }

void Backend::launchTestCard() {
    if (engine_) return;
    const QString exe = QCoreApplication::applicationDirPath() + QStringLiteral("/zen1-engine");
    if (!QFileInfo(exe).isExecutable()) {
        setMessage(QStringLiteral("Engine not found next to the frontend: ") + exe);
        return;
    }
    engine_ = new QProcess(this);
    engine_->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(engine_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const bool crashed = status == QProcess::CrashExit;
        zen1::log::info("engine finished, code " + std::to_string(code) + (crashed ? " (crashed)" : ""));
        if (crashed) setMessage(QStringLiteral("The engine stopped unexpectedly. You are back in the menu."));
        engine_->deleteLater();
        engine_ = nullptr;
        input_.refresh(controlsFile_);  // panels may have been replugged meanwhile
        emit changed();
    });
    QStringList args{QStringLiteral("--test-card")};
    if (server_.isListening()) args << QStringLiteral("--ipc") << server_.fullServerName();
    setMessage(QStringLiteral("Starting the test card..."));
    engine_->start(exe, args);
    emit changed();
}

void Backend::onEngineLine(const QString& line) {
    zen1::log::info("engine: " + line.toStdString());
    if (line == QStringLiteral("ready")) setMessage(QString());
    else if (line.startsWith(QStringLiteral("error"))) setMessage(QStringLiteral("Engine error: ") + line.mid(6));
}

void Backend::checkExitHold() {
    // Leave Zen1 Arcade: hold any button on both panels for 3 seconds
    // (only in the menu; the engine has its own exit while a game runs).
    const bool holding = !engine_ && input_.anyHeld(0) && input_.anyHeld(1);
    double progress = 0.0;
    if (holding) {
        if (!holdClock_.isValid()) holdClock_.start();
        progress = static_cast<double>(holdClock_.elapsed()) / 3000.0;
    } else {
        holdClock_.invalidate();
    }
    if (progress != exitProgress_) {
        exitProgress_ = progress;
        emit exitProgressChanged();
    }
    if (progress >= 1.0) emit quitRequested();
}
