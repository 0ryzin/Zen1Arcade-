// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend.hpp"

#include "zen1/data_root.hpp"
#include "zen1/input/assignment.hpp"
#include "zen1/input/bindings.hpp"
#include "zen1/log.hpp"
#include "zen1/settings.hpp"
#include "zen1/version.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace {

const char* const kStepNames[7] = {"LIGHT PUNCH (LP)", "MEDIUM PUNCH (MP)", "HEAVY PUNCH (HP)",
                                   "LIGHT KICK (LK)", "MEDIUM KICK (MK)", "HEAVY KICK (HK)",
                                   "PLAYER BUTTON (coin / start)"};

zen1::Settings loadSettings(const QString& f) {
    zen1::Settings s;
    s.load(f.toStdString());
    return s;
}

}  // namespace

Backend::Backend(QObject* parent) : QObject(parent) {
    connect(&input_, &InputWatcher::assignmentChanged, this, &Backend::changed);
    connect(&input_, &InputWatcher::control, this, [this](int player, const QString& name) {
        if (!engine_ && captureMode_ == CaptureMode::None) emit control(player, name);
    });
    connect(&input_, &InputWatcher::captured, this, &Backend::onCaptured);
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
    auto q = [](const std::filesystem::path& p) { return QString::fromStdString(p.string()); };
    dataRoot_ = q(paths.root);
    gamesDir_ = q(paths.games());
    configsDir_ = q(paths.configs());
    controlsFile_ = q(paths.controlsFile());
    settingsFile_ = q(paths.settingsFile());
    displayFile_ = q(paths.displayFile());
    libraryFile_ = q(paths.configs() / "library.toml");
    artworkDir_ = q(paths.artwork());
    if (engineDir_.isEmpty()) {
        // Installed / AppImage: next to the frontend. Build tree: ../engine.
        engineDir_ = qEnvironmentVariable("ZEN1_ENGINE_DIR", QCoreApplication::applicationDirPath());
        const QString buildTree = QCoreApplication::applicationDirPath() + QStringLiteral("/../engine");
        if (!QFileInfo(engineDir_ + QStringLiteral("/zen1-engine")).isExecutable() &&
            QFileInfo(buildTree + QStringLiteral("/zen1-engine")).isExecutable())
            engineDir_ = QDir(buildTree).canonicalPath();
    }
    if (!r.ok) {
        if (error) *error = QString::fromStdString(r.error);
        return false;
    }
    zen1::log::openSessionLog(paths.logs(), "frontend");
    zen1::log::info(std::string("zen1-arcade ") + zen1::kVersion + " data folder " + paths.root.string());

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
            while (link_ && link_->canReadLine()) onEngineLine(QString::fromUtf8(link_->readLine()).trimmed());
        });
    });

    input_.refresh(controlsFile_);
    rescan();
    emit changed();
    emit settingsChanged();
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

// ---------------------------------------------------------------- library

void Backend::rescan() {
    QProcess p;
    p.start(engineDir_ + QStringLiteral("/zen1-engine"),
            {QStringLiteral("--scan"), QStringLiteral("--games-dir"), gamesDir_});
    if (!p.waitForFinished(15000) || p.exitCode() != 0) {
        setMessage(QStringLiteral("Could not read the game list from zen1-engine."));
        return;
    }
    const auto doc = QJsonDocument::fromJson(p.readAllStandardOutput());
    games_.clear();
    for (const auto& v : doc.object().value(QStringLiteral("games")).toArray()) {
        QVariantMap g = v.toObject().toVariantMap();
        const QString set = g.value(QStringLiteral("set")).toString();
        // Artwork the user added: Artwork/<set>/<kind>.png|jpg, or Artwork/<set>.png|jpg
        QString art;
        for (const QString& kind : {QStringLiteral("card"), QStringLiteral("flyer"), QStringLiteral("boxart"),
                                    QStringLiteral("title"), QStringLiteral("snap")})
            for (const QString& ext : {QStringLiteral(".png"), QStringLiteral(".jpg")})
                if (art.isEmpty() && QFileInfo::exists(artworkDir_ + "/" + set + "/" + kind + ext))
                    art = artworkDir_ + "/" + set + "/" + kind + ext;
        for (const QString& ext : {QStringLiteral(".png"), QStringLiteral(".jpg")})
            if (art.isEmpty() && QFileInfo::exists(artworkDir_ + "/" + set + ext)) art = artworkDir_ + "/" + set + ext;
        g.insert(QStringLiteral("artwork"), art.isEmpty() ? QString() : QUrl::fromLocalFile(art).toString());
        games_.push_back(g);
    }
    loadLibraryState();
}

void Backend::loadLibraryState() {
    const auto s = loadSettings(libraryFile_);
    for (auto& v : games_) {
        QVariantMap g = v.toMap();
        const std::string set = g.value(QStringLiteral("set")).toString().toStdString();
        g.insert(QStringLiteral("favourite"), s.getBool("favourites." + set, false));
        g.insert(QStringLiteral("lastPlayed"), static_cast<double>(s.getInt("recent." + set, 0)));
        g.insert(QStringLiteral("plays"), static_cast<int>(s.getInt("plays." + set, 0)));
        v = g;
    }
    // Supported and ready first, then supported but missing files, then
    // everything else; alphabetical inside each group.
    std::stable_sort(games_.begin(), games_.end(), [](const QVariant& a, const QVariant& b) {
        auto rank = [](const QVariantMap& g) {
            const QString st = g.value(QStringLiteral("status")).toString();
            return st == QStringLiteral("ready") ? 0 : st == QStringLiteral("unsupported") ? 2 : 1;
        };
        const auto ma = a.toMap(), mb = b.toMap();
        if (rank(ma) != rank(mb)) return rank(ma) < rank(mb);
        return ma.value(QStringLiteral("title")).toString().compare(mb.value(QStringLiteral("title")).toString(),
                                                                    Qt::CaseInsensitive) < 0;
    });
    emit gamesChanged();
}

void Backend::useDemoLibrary() {
    // Fixed data for automated screenshots: real titles, invented states.
    auto g = [](const char* set, const char* title, const char* year, const char* sys, const char* status,
                bool fav, int rot) {
        QVariantMap m;
        m[QStringLiteral("set")] = QString::fromLatin1(set);
        m[QStringLiteral("title")] = QString::fromUtf8(title);
        m[QStringLiteral("year")] = QString::fromLatin1(year);
        m[QStringLiteral("manufacturer")] = QStringLiteral("Capcom");
        m[QStringLiteral("system")] = QString::fromLatin1(sys);
        m[QStringLiteral("status")] = QString::fromLatin1(status);
        m[QStringLiteral("supported")] = QString::fromLatin1(status) != QStringLiteral("unsupported");
        m[QStringLiteral("players")] = 2;
        m[QStringLiteral("buttons")] = 6;
        m[QStringLiteral("rotation")] = rot;
        m[QStringLiteral("favourite")] = fav;
        m[QStringLiteral("lastPlayed")] = fav ? 1.0 : 0.0;
        m[QStringLiteral("plays")] = 0;
        m[QStringLiteral("artwork")] = QString();
        m[QStringLiteral("missing")] = QStringList();
        m[QStringLiteral("reason")] = QStringLiteral("This game's hardware is not implemented in the Zen1 engine yet");
        return QVariant(m);
    };
    games_ = {g("sf2ce", "Street Fighter II': Champion Edition", "1992", "Capcom CPS1", "ready", true, 0),
              g("sf2", "Street Fighter II: The World Warrior", "1991", "Capcom CPS1", "ready", false, 0),
              g("ffight", "Final Fight", "1989", "Capcom CPS1", "ready", true, 0),
              g("1941", "1941: Counter Attack", "1990", "Capcom CPS1", "missing", false, 270),
              g("mslug", "mslug", "", "", "unsupported", false, 0),
              g("pacman", "pacman", "", "", "unsupported", false, 0),
              g("mk2", "mk2", "", "", "unsupported", false, 0)};
    emit gamesChanged();
}

void Backend::toggleFavourite(const QString& set) {
    auto s = loadSettings(libraryFile_);
    const std::string key = "favourites." + set.toStdString();
    if (s.getBool(key, false)) s.remove(key);
    else s.set(key, true);
    s.save(libraryFile_.toStdString());
    for (auto& v : games_) {
        QVariantMap g = v.toMap();
        if (g.value(QStringLiteral("set")).toString() == set) {
            g[QStringLiteral("favourite")] = s.getBool(key, false);
            v = g;
        }
    }
    emit gamesChanged();
}

// ---------------------------------------------------------------- settings

QVariantMap Backend::settings() const {
    const auto s = loadSettings(settingsFile_);
    const auto d = loadSettings(displayFile_);
    QVariantMap m;
    m[QStringLiteral("introEnabled")] = s.getBool("intro.enabled", true);
    m[QStringLiteral("introSeconds")] = static_cast<int>(s.getInt("intro.seconds", 6));
    m[QStringLiteral("introSound")] = s.getBool("intro.sound", true);
    m[QStringLiteral("introSkippable")] = s.getBool("intro.skippable", true);
    m[QStringLiteral("scale")] = QString::fromStdString(d.getString("game.scale", "original"));
    m[QStringLiteral("bezel")] = QString::fromStdString(d.getString("game.bezel", "neon"));
    m[QStringLiteral("sync")] = QString::fromStdString(d.getString("game.sync", "display"));
    return m;
}

void Backend::setSetting(const QString& key, const QVariant& value) {
    const bool displayKey = key.startsWith(QStringLiteral("game."));
    const QString file = displayKey ? displayFile_ : settingsFile_;
    auto s = loadSettings(file);
    const std::string k = key.toStdString();
    switch (value.typeId()) {
    case QMetaType::Bool: s.set(k, value.toBool()); break;
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::Double: s.set(k, static_cast<long long>(value.toLongLong())); break;
    default: s.set(k, value.toString().toStdString()); break;
    }
    std::string err;
    if (!s.save(file.toStdString(), &err)) setMessage(QStringLiteral("Could not save setting: ") + QString::fromStdString(err));
    emit settingsChanged();
}

void Backend::playIntroSound(double seconds) {
    if (!introAudio_.play(seconds)) zen1::log::warn("intro sound unavailable");
}
void Backend::stopIntroSound() { introAudio_.fadeOut(); }

// ---------------------------------------------------------------- engine

void Backend::startEngine(const QStringList& extra, const QString& title, const QString& set) {
    if (engine_) return;
    const QString exe = engineDir_ + QStringLiteral("/zen1-engine");
    if (!QFileInfo(exe).isExecutable()) {
        setMessage(QStringLiteral("Engine not found next to the frontend: ") + exe);
        return;
    }
    stopIntroSound();
    engine_ = new QProcess(this);
    engine_->setProcessChannelMode(QProcess::SeparateChannels);
    engineError_.clear();
    connect(engine_, &QProcess::readyReadStandardError, this, [this] {
        engineError_ += QString::fromUtf8(engine_->readAllStandardError());
    });
    connect(engine_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const bool crashed = status == QProcess::CrashExit;
        zen1::log::info("engine finished, code " + std::to_string(code) + (crashed ? " (crashed)" : ""));
        if (crashed) {
            setMessage(QStringLiteral("The engine stopped unexpectedly. You are back in the library."));
        } else if (code == 3) {
            QString e = engineError_.trimmed();
            e.remove(QStringLiteral("zen1-engine: "));
            setMessage(e.isEmpty() ? QStringLiteral("The game could not start.") : e);
        } else if (code != 0) {
            setMessage(QStringLiteral("The engine could not open the screen (code %1). See Logs.").arg(code));
        } else {
            setMessage(QString());
        }
        if (!runningSet_.isEmpty() && code == 0 && !crashed && engineClock_.elapsed() > 5000) {
            auto s = loadSettings(libraryFile_);
            const std::string set = runningSet_.toStdString();
            s.set("recent." + set, static_cast<long long>(QDateTime::currentSecsSinceEpoch()));
            s.set("plays." + set, s.getInt("plays." + set, 0) + 1);
            s.save(libraryFile_.toStdString());
            loadLibraryState();
        }
        engine_->deleteLater();
        engine_ = nullptr;
        runningSet_.clear();
        runningTitle_.clear();
        input_.setPaused(false);  // also re-scans: panels may have been replugged
        emit changed();
        emit gameExited();
    });
    QStringList args = extra;
    if (server_.isListening()) args << QStringLiteral("--ipc") << server_.fullServerName();
    runningTitle_ = title;
    runningSet_ = set;
    input_.setPaused(true);
    engineClock_.start();
    zen1::log::info("starting engine: " + args.join(QLatin1Char(' ')).toStdString());
    engine_->start(exe, args);
    emit changed();
    emit gameStarted();
}

void Backend::launchGame(const QString& set) {
    for (const auto& v : games_) {
        const QVariantMap g = v.toMap();
        if (g.value(QStringLiteral("set")).toString() != set) continue;
        const QString status = g.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("unsupported")) {
            setMessage(g.value(QStringLiteral("title")).toString() + QStringLiteral(": ") +
                       g.value(QStringLiteral("reason")).toString());
            return;
        }
        if (status != QStringLiteral("ready")) {
            QStringList missing = g.value(QStringLiteral("missing")).toStringList();
            setMessage(status == QStringLiteral("missing")
                           ? QStringLiteral("Put %1.zip (current MAME version) in %2").arg(set, gamesDir_)
                           : QStringLiteral("%1.zip is incomplete or a different version. Missing: %2")
                                 .arg(set, missing.mid(0, 4).join(QStringLiteral(", "))));
            return;
        }
        startEngine({QStringLiteral("--game"), set, QStringLiteral("--games-dir"), gamesDir_},
                    g.value(QStringLiteral("title")).toString(), set);
        return;
    }
}

void Backend::launchTestCard() { startEngine({QStringLiteral("--test-card")}, QStringLiteral("Test card"), {}); }

void Backend::onEngineLine(const QString& line) {
    zen1::log::info("engine: " + line.toStdString());
    if (line.startsWith(QStringLiteral("error "))) setMessage(QStringLiteral("Engine: ") + line.mid(6));
}

void Backend::checkExitHold() {
    // Leave Zen1 Arcade from the library: hold both player buttons for 3 s.
    const bool holding = !engine_ && input_.playerButtonHeld(0) && input_.playerButtonHeld(1);
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

// ---------------------------------------------------------------- controls

void Backend::refreshControllers() {
    input_.refresh(controlsFile_);
    emit changed();
}

void Backend::startAssign(int player) {
    captureMode_ = CaptureMode::Assign;
    capturePlayer_ = player;
    capturePrompt_ = QStringLiteral("PLAYER %1: press any button on the panel for Player %1").arg(player + 1);
    input_.beginCapture();
    emit captureChanged();
}

void Backend::startRemap(int player) {
    if (!input_.assigned(player)) {
        setMessage(QStringLiteral("Assign a panel to Player %1 first.").arg(player + 1));
        return;
    }
    // Only the device currently assigned to this player counts.
    const auto devices = zen1::input::scanDevices();
    const auto a = zen1::input::assignPlayers(devices, zen1::input::loadPlayerBindings(controlsFile_.toStdString()));
    const auto& slot = a.players[static_cast<size_t>(player)];
    captureDevice_ = -1;
    input_.beginCapture();
    if (slot.device) {
        for (int i = 0;; ++i) {
            const auto* d = input_.captureDevice(i);
            if (!d) break;
            if (d->eventPath == devices[*slot.device].eventPath) captureDevice_ = i;
        }
    }
    if (captureDevice_ < 0) {
        input_.endCapture();
        setMessage(QStringLiteral("Player %1's panel is not connected.").arg(player + 1));
        return;
    }
    captureMode_ = CaptureMode::Remap;
    capturePlayer_ = player;
    captureStep_ = 0;
    captureButtons_.clear();
    capturePrompt_ = QStringLiteral("PLAYER %1: press %2").arg(player + 1).arg(QString::fromLatin1(kStepNames[0]));
    emit captureChanged();
}

void Backend::onCaptured(int deviceIndex, int buttonIndex) {
    const auto* d = input_.captureDevice(deviceIndex);
    if (!d) return;
    if (captureMode_ == CaptureMode::Assign) {
        auto ids = zen1::input::loadPlayerBindings(controlsFile_.toStdString());
        const auto id = zen1::input::DeviceIdentity::from(*d);
        const int other = 1 - capturePlayer_;
        if (ids[static_cast<size_t>(other)].byPath == id.byPath && !id.byPath.empty())
            ids[static_cast<size_t>(other)] = {};  // a panel belongs to one player only
        ids[static_cast<size_t>(capturePlayer_)] = id;
        std::string err;
        if (!zen1::input::savePlayerBindings(controlsFile_.toStdString(), ids, &err)) {
            finishCapture(QStringLiteral("Could not save: ") + QString::fromStdString(err));
            return;
        }
        finishCapture(QStringLiteral("Player %1 is now %2").arg(capturePlayer_ + 1).arg(QString::fromStdString(d->name)));
    } else if (captureMode_ == CaptureMode::Remap) {
        if (deviceIndex != captureDevice_) return;
        if (std::find(captureButtons_.begin(), captureButtons_.end(), buttonIndex) != captureButtons_.end()) {
            capturePrompt_ = QStringLiteral("That button is already used. PLAYER %1: press %2")
                                 .arg(capturePlayer_ + 1)
                                 .arg(QString::fromLatin1(kStepNames[captureStep_]));
            emit captureChanged();
            return;
        }
        captureButtons_.push_back(buttonIndex);
        if (++captureStep_ < 7) {
            capturePrompt_ = QStringLiteral("PLAYER %1: press %2")
                                 .arg(capturePlayer_ + 1)
                                 .arg(QString::fromLatin1(kStepNames[captureStep_]));
            emit captureChanged();
            return;
        }
        auto maps = zen1::input::loadButtonMaps(controlsFile_.toStdString());
        auto& m = maps[static_cast<size_t>(capturePlayer_)];
        for (size_t i = 0; i < 6; ++i) m.action[i] = captureButtons_[i];
        m.player = captureButtons_[6];
        std::string err;
        if (!zen1::input::saveButtonMaps(controlsFile_.toStdString(), maps, &err)) {
            finishCapture(QStringLiteral("Could not save: ") + QString::fromStdString(err));
            return;
        }
        finishCapture(QStringLiteral("Player %1 buttons saved.").arg(capturePlayer_ + 1));
    }
}

void Backend::finishCapture(const QString& msg) {
    captureMode_ = CaptureMode::None;
    capturePrompt_.clear();
    input_.endCapture();
    setMessage(msg);
    emit captureChanged();
    emit changed();
}

void Backend::cancelCapture() {
    if (captureMode_ != CaptureMode::None) finishCapture(QStringLiteral("Cancelled."));
}

void Backend::swapPlayers() {
    auto ids = zen1::input::loadPlayerBindings(controlsFile_.toStdString());
    auto maps = zen1::input::loadButtonMaps(controlsFile_.toStdString());
    if (ids[0].empty() && ids[1].empty()) {
        setMessage(QStringLiteral("Nothing to swap yet: assign the panels first."));
        return;
    }
    std::swap(ids[0], ids[1]);
    std::swap(maps[0], maps[1]);
    zen1::input::savePlayerBindings(controlsFile_.toStdString(), ids);
    zen1::input::saveButtonMaps(controlsFile_.toStdString(), maps);
    refreshControllers();
    setMessage(QStringLiteral("Player 1 and Player 2 swapped."));
}

void Backend::resetControls() {
    zen1::input::savePlayerBindings(controlsFile_.toStdString(), {});
    zen1::input::saveButtonMaps(controlsFile_.toStdString(), {});
    refreshControllers();
    setMessage(QStringLiteral("Controls reset: panels are found by name again, buttons use the default order."));
}
