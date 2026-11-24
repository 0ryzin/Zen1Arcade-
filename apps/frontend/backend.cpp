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
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
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
    connect(&input_, &InputWatcher::liveChanged, this, [this](int) { emit liveChanged(); });
    calTimer_.setInterval(16);
    connect(&calTimer_, &QTimer::timeout, this, &Backend::calibrationTick);
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
    selectedSet_ = QString::fromStdString(loadSettings(libraryFile_).getString("ui.selected"));
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
        // The library shows the games that are actually in the Games folder.
        if (g.value(QStringLiteral("status")).toString() == QStringLiteral("missing")) continue;
        games_.push_back(g);
    }
    // One big alphabetical library; supported and unsupported together.
    std::stable_sort(games_.begin(), games_.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("title")).toString().compare(
                   b.toMap().value(QStringLiteral("title")).toString(), Qt::CaseInsensitive) < 0;
    });
    applyArtwork();
    if (settings().value(QStringLiteral("artworkDownload")).toBool()) downloadArtwork();
}

// Images for a game, best first: the user's own files, then downloaded ones.
QStringList Backend::artworkCandidates(const QString& set) const {
    QStringList out;
    const QStringList exts = {QStringLiteral(".png"), QStringLiteral(".jpg")};
    for (const QString& kind : {QStringLiteral("card"), QStringLiteral("flyer"), QStringLiteral("boxart"),
                                QStringLiteral("title"), QStringLiteral("snap"), QStringLiteral("marquee")})
        for (const QString& ext : exts) {
            const QString f = artworkDir_ + QLatin1Char('/') + set + QLatin1Char('/') + kind + ext;
            if (QFileInfo::exists(f)) out << f;
        }
    for (const QString& ext : exts)
        if (QFileInfo::exists(artworkDir_ + QLatin1Char('/') + set + ext)) out << artworkDir_ + QLatin1Char('/') + set + ext;
    for (const QString& kind : {QStringLiteral("flyers"), QStringLiteral("titles"), QStringLiteral("ingames"),
                                QStringLiteral("marquees")}) {
        const QString f = artworkDir_ + QStringLiteral("/downloaded/") + kind + QLatin1Char('/') + set + QStringLiteral(".png");
        if (QFileInfo::exists(f) && QFileInfo(f).size() > 0) out << f;
    }
    return out;
}

void Backend::applyArtwork() {
    const auto lib = loadSettings(libraryFile_);
    for (auto& v : games_) {
        QVariantMap g = v.toMap();
        const QString set = g.value(QStringLiteral("set")).toString();
        const QStringList c = artworkCandidates(set);
        const QString chosen = QString::fromStdString(lib.getString("artwork." + set.toStdString()));
        QString art = c.isEmpty() ? QString() : c.first();
        for (const QString& f : c)
            if (!chosen.isEmpty() && f.endsWith(chosen)) art = f;
        g.insert(QStringLiteral("artwork"), art.isEmpty() ? QString() : QUrl::fromLocalFile(art).toString());
        g.insert(QStringLiteral("artworkCount"), static_cast<int>(c.size()));
        v = g;
    }
    emit gamesChanged();
}

void Backend::nextArtwork(const QString& set) {
    const QStringList c = artworkCandidates(set);
    if (c.size() < 2) {
        setMessage(c.isEmpty() ? QStringLiteral("No artwork for this game yet.")
                               : QStringLiteral("This game has only one image."));
        return;
    }
    auto lib = loadSettings(libraryFile_);
    const QString chosen = QString::fromStdString(lib.getString("artwork." + set.toStdString()));
    qsizetype i = 0;
    for (qsizetype k = 0; k < c.size(); ++k)
        if (!chosen.isEmpty() && c[k].endsWith(chosen)) i = k;
    const QString next = c[(i + 1) % c.size()];
    lib.set("artwork." + set.toStdString(), QDir(artworkDir_).relativeFilePath(next).toStdString());
    lib.save(libraryFile_.toStdString());
    applyArtwork();
}

void Backend::resetArtwork(const QString& set) {
    auto lib = loadSettings(libraryFile_);
    lib.remove("artwork." + set.toStdString());
    lib.save(libraryFile_.toStdString());
    applyArtwork();
    setMessage(QStringLiteral("Artwork reset to automatic."));
}

void Backend::downloadArtwork() {
    // Only for games Zen1 knows; never re-downloads what is already cached
    // (a failed download leaves an empty marker so it is not retried).
    downloadQueue_.clear();
    for (const auto& v : games_) {
        const QVariantMap g = v.toMap();
        if (!g.contains(QStringLiteral("year")) && g.value(QStringLiteral("status")).toString() == QStringLiteral("unsupported"))
            continue;  // unrecognised file: no name to look up
        const QString set = g.value(QStringLiteral("set")).toString();
        for (const QString& kind : {QStringLiteral("flyers"), QStringLiteral("titles"), QStringLiteral("ingames")}) {
            const QString f = artworkDir_ + QStringLiteral("/downloaded/") + kind + QLatin1Char('/') + set + QStringLiteral(".png");
            if (!QFileInfo::exists(f)) downloadQueue_ << set + QLatin1Char('|') + kind;
        }
    }
    if (downloadQueue_.isEmpty()) {
        artworkStatus_ = QStringLiteral("Artwork is up to date.");
        emit artworkChanged();
        return;
    }
    downloadNext();
}

void Backend::downloadNext() {
    if (downloadQueue_.isEmpty()) {
        artworkStatus_ = QStringLiteral("Artwork download finished.");
        emit artworkChanged();
        applyArtwork();
        return;
    }
    const QStringList job = downloadQueue_.takeFirst().split(QLatin1Char('|'));
    const QString set = job[0], kind = job[1];
    QString tmpl = QString::fromStdString(loadSettings(settingsFile_).getString(
        "artwork.source", "https://adb.arcadeitalia.net/media/mame.current/{kind}/{set}.png"));
    tmpl.replace(QStringLiteral("{kind}"), kind).replace(QStringLiteral("{set}"), set);
    artworkStatus_ = QStringLiteral("Downloading artwork: %1 (%2 left)").arg(set).arg(downloadQueue_.size());
    emit artworkChanged();
    QNetworkRequest req{QUrl(tmpl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Zen1Arcade/") + version());
    req.setTransferTimeout(15000);
    QNetworkReply* reply = net_.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, set, kind] {
        reply->deleteLater();
        const QString dir = artworkDir_ + QStringLiteral("/downloaded/") + kind;
        QDir().mkpath(dir);
        QSaveFile f(dir + QLatin1Char('/') + set + QStringLiteral(".png"));
        const QByteArray data = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentNotFoundError) {
            // Network down: stop quietly and try again next time.
            downloadQueue_.clear();
            artworkStatus_ = QStringLiteral("Artwork download unavailable (offline?)");
            emit artworkChanged();
            applyArtwork();
            return;
        }
        if (f.open(QIODevice::WriteOnly)) {
            if (data.startsWith("\x89PNG")) f.write(data);  // else: empty marker, not found
            f.commit();
        }
        QTimer::singleShot(150, this, &Backend::downloadNext);  // be gentle with the server
    });
}

void Backend::setSelectedSet(const QString& s) {
    if (s == selectedSet_) return;
    selectedSet_ = s;
    auto lib = loadSettings(libraryFile_);
    lib.set("ui.selected", s.toStdString());
    lib.save(libraryFile_.toStdString());
    emit selectedChanged();
}

void Backend::useDemoLibrary() {
    // Fixed data for automated screenshots: real titles and details.
    auto g = [](const char* set, const char* title, const char* year, const char* maker, const char* sys, bool supported,
                int players, const char* joy, QStringList buttons, int rot) {
        QVariantMap m;
        m[QStringLiteral("set")] = QString::fromLatin1(set);
        m[QStringLiteral("title")] = QString::fromUtf8(title);
        m[QStringLiteral("year")] = QString::fromLatin1(year);
        m[QStringLiteral("manufacturer")] = QString::fromUtf8(maker);
        m[QStringLiteral("system")] = QString::fromLatin1(sys);
        m[QStringLiteral("status")] = supported ? QStringLiteral("ready") : QStringLiteral("unsupported");
        m[QStringLiteral("supported")] = supported;
        m[QStringLiteral("players")] = players;
        m[QStringLiteral("joystick")] = QString::fromLatin1(joy);
        m[QStringLiteral("buttonNames")] = buttons;
        m[QStringLiteral("buttons")] = static_cast<int>(buttons.size());
        m[QStringLiteral("rotation")] = rot;
        m[QStringLiteral("format")] = QStringLiteral("ZIP ROM set");
        m[QStringLiteral("artwork")] = QString();
        if (!supported) m[QStringLiteral("reason")] = QStringLiteral("Zen1 does not emulate this yet");
        return QVariant(m);
    };
    const QStringList six = {QStringLiteral("Light Punch"), QStringLiteral("Medium Punch"), QStringLiteral("Heavy Punch"),
                             QStringLiteral("Light Kick"), QStringLiteral("Medium Kick"), QStringLiteral("Heavy Kick")};
    games_ = {g("1941", "1941: Counter Attack", "1990", "Capcom", "Capcom CPS-1", true, 2, "8-way", {QStringLiteral("Shot"), QStringLiteral("Loop")}, 270),
              g("ffight", "Final Fight", "1989", "Capcom", "Capcom CPS-1", true, 2, "8-way", {QStringLiteral("Attack"), QStringLiteral("Jump")}, 0),
              g("ghouls", "Ghouls'n Ghosts", "1988", "Capcom", "Capcom CPS-1", true, 2, "8-way", {QStringLiteral("Fire"), QStringLiteral("Jump")}, 0),
              g("mslug", "Metal Slug", "1996", "Nazca / SNK", "SNK Neo Geo MVS", false, 2, "8-way", {}, 0),
              g("mspacman", "Ms. Pac-Man", "1981", "Midway / General Computer Corporation", "Namco Pac-Man", true, 2, "4-way", {}, 90),
              g("mk2", "Mortal Kombat II", "1993", "Midway", "Midway T-Unit", false, 2, "8-way", {}, 0),
              g("pacman", "Pac-Man", "1980", "Namco (Midway license)", "Namco Pac-Man", true, 2, "4-way", {}, 90),
              g("sf2", "Street Fighter II: The World Warrior", "1991", "Capcom", "Capcom CPS-1", true, 2, "8-way", six, 0),
              g("sf2ce", "Street Fighter II': Champion Edition", "1992", "Capcom", "Capcom CPS-1", true, 2, "8-way", six, 0),
              g("strider", "Strider", "1989", "Capcom", "Capcom CPS-1", true, 2, "8-way", {QStringLiteral("Attack"), QStringLiteral("Jump")}, 0),
              g("tmnt", "Teenage Mutant Ninja Turtles", "1989", "Konami", "Konami TMNT", false, 4, "8-way", {}, 0)};
    selectedSet_ = QStringLiteral("sf2ce");
    emit gamesChanged();
    emit selectedChanged();
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
    m[QStringLiteral("insertCoin")] = s.getBool("insertcoin.enabled", true);
    m[QStringLiteral("insertCoinSound")] = s.getBool("insertcoin.sound", true);
    m[QStringLiteral("menuSounds")] = s.getBool("sound.menu", true);
    m[QStringLiteral("menuVolume")] = static_cast<int>(s.getInt("sound.menu_volume", 70));
    m[QStringLiteral("artworkDownload")] = s.getBool("artwork.download", true);
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

void Backend::sfx(const QString& name) {
    const auto st = settings();
    const bool always = name == QStringLiteral("coin");
    if (!always && !st.value(QStringLiteral("menuSounds")).toBool()) return;
    if (name == QStringLiteral("ambience") && !st.value(QStringLiteral("insertCoinSound")).toBool()) return;
    if (always && !st.value(QStringLiteral("insertCoinSound")).toBool() && !st.value(QStringLiteral("menuSounds")).toBool())
        return;
    sfx_.play(name.toStdString(), static_cast<float>(st.value(QStringLiteral("menuVolume")).toInt()) / 100.0f);
}

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
            sfx(QStringLiteral("error"));
            setMessage(QStringLiteral("NOT SUPPORTED: ") + g.value(QStringLiteral("reason")).toString());
            return;
        }
        if (status != QStringLiteral("ready")) {
            sfx(QStringLiteral("error"));
            setMessage(QStringLiteral("%1.zip is incomplete or a different version (current MAME sets are needed). Missing: %2")
                           .arg(set, g.value(QStringLiteral("missing")).toStringList().mid(0, 4).join(QStringLiteral(", "))));
            return;
        }
        setSelectedSet(set);
        startEngine({QStringLiteral("--game"), set, QStringLiteral("--games-dir"), gamesDir_},
                    g.value(QStringLiteral("title")).toString(), set);
        return;
    }
}

void Backend::launchTestCard() { startEngine({QStringLiteral("--test-card")}, QStringLiteral("Test card"), {}); }

void Backend::onEngineLine(const QString& line) {
    zen1::log::info("engine: " + line.toStdString());
    if (line.startsWith(QStringLiteral("error "))) setMessage(QStringLiteral("Engine: ") + line.mid(6));
    else if (line == QStringLiteral("ready")) emit engineReady();
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

void Backend::unassign(int player) {
    auto ids = zen1::input::loadPlayerBindings(controlsFile_.toStdString());
    ids[static_cast<size_t>(player == 1 ? 1 : 0)] = {};
    zen1::input::savePlayerBindings(controlsFile_.toStdString(), ids);
    refreshControllers();
    setMessage(input_.assigned(player)
                   ? QStringLiteral("Player %1 cleared, but the panel's own name still says Player %1, so it was found again.").arg(player + 1)
                   : QStringLiteral("Player %1 has no panel now.").arg(player + 1));
}

QVariantList Backend::p1Map() const {
    const auto m = input_.buttonMap(0);
    QVariantList l;
    for (int i : m.action) l << i;
    l << m.player;
    return l;
}

QVariantList Backend::devices() const {
    // Every input device Linux reports, with what Zen1 makes of it.
    QVariantList out;
    const auto devs = zen1::input::scanDevices();
    const auto a = zen1::input::assignPlayers(devs, zen1::input::loadPlayerBindings(controlsFile_.toStdString()));
    for (size_t i = 0; i < devs.size(); ++i) {
        const auto& d = devs[i];
        const bool candidate = zen1::input::isPlayerCandidate(d);
        if (!candidate && !d.valve) continue;  // keyboards, mice, power buttons...
        QVariantMap m;
        m[QStringLiteral("name")] = QString::fromStdString(d.name);
        m[QStringLiteral("ids")] = QString::fromStdString(zen1::input::hex4(d.vendor) + ":" + zen1::input::hex4(d.product));
        m[QStringLiteral("serial")] = QString::fromStdString(d.uniq);
        m[QStringLiteral("port")] = QString::fromStdString(d.byPath.substr(d.byPath.find_last_of('/') + 1));
        int player = 0;
        for (int p = 0; p < 2; ++p)
            if (a.players[static_cast<size_t>(p)].device == i) player = p + 1;
        m[QStringLiteral("player")] = player;
        m[QStringLiteral("note")] = d.valve || d.virtualDevice ? QStringLiteral("Steam Deck controls: never a cabinet player")
                                    : player ? QStringLiteral("Player %1").arg(player) : QStringLiteral("not assigned");
        out << m;
    }
    return out;
}

QVariantMap Backend::liveMap(int player) const {
    const auto l = input_.live(player);
    QVariantMap m;
    m[QStringLiteral("connected")] = l.connected;
    m[QStringLiteral("name")] = QString::fromStdString(l.name);
    m[QStringLiteral("dirX")] = l.dirX;
    m[QStringLiteral("dirY")] = l.dirY;
    double x = 0, y = 0;
    zen1::input::calibratedPosition(l.calibration, l.nx, l.ny, x, y);
    m[QStringLiteral("rawX")] = l.nx;
    m[QStringLiteral("rawY")] = l.ny;
    m[QStringLiteral("x")] = x;
    m[QStringLiteral("y")] = y;
    m[QStringLiteral("hasAxes")] = l.hasAxes;
    m[QStringLiteral("hasHat")] = l.hasHat;
    m[QStringLiteral("calibrated")] = l.calibration.calibrated;
    m[QStringLiteral("digital")] = l.calibration.digital;
    m[QStringLiteral("deadzone")] = l.calibration.calibrated && !l.calibration.digital ? l.calibration.deadzone : 0.5;
    QVariantList b;
    for (bool v : l.buttons) b << v;
    m[QStringLiteral("buttons")] = b;
    const auto map = input_.buttonMap(player);
    QVariantList ml;
    for (int i : map.action) ml << i;
    ml << map.player;
    m[QStringLiteral("map")] = ml;
    return m;
}

// ---------------------------------------------------------------- calibration

QVariantMap Backend::calibration() const {
    QVariantMap m;
    m[QStringLiteral("player")] = calPlayer_;
    m[QStringLiteral("step")] = calStep_;
    m[QStringLiteral("seconds")] = calClock_.isValid() ? static_cast<double>(calClock_.elapsed()) / 1000.0 : 0.0;
    m[QStringLiteral("deadzone")] = calResult_.deadzone;
    m[QStringLiteral("digital")] = calResult_.digital;
    m[QStringLiteral("minX")] = calResult_.minX;
    m[QStringLiteral("maxX")] = calResult_.maxX;
    m[QStringLiteral("minY")] = calResult_.minY;
    m[QStringLiteral("maxY")] = calResult_.maxY;
    m[QStringLiteral("centerX")] = calResult_.centerX;
    m[QStringLiteral("centerY")] = calResult_.centerY;
    return m;
}

void Backend::calibrationBegin(int player) {
    const auto l = input_.live(player);
    if (!l.connected) {
        sfx(QStringLiteral("error"));
        setMessage(QStringLiteral("Player %1 has no panel connected to calibrate.").arg(player + 1));
        return;
    }
    calPlayer_ = player;
    calResult_ = l.calibration;
    if (!calResult_.calibrated) calResult_.deadzone = 0.5;
    if (!l.hasAxes) {
        // Hat-only stick: switches report directions directly. Nothing to
        // calibrate; the test step shows every direction.
        calResult_ = {};
        calResult_.calibrated = true;
        calResult_.digital = true;
        calStep_ = QStringLiteral("test");
    } else {
        calStep_ = QStringLiteral("intro");
    }
    input_.setStickSuppressed(player, true);
    emit calibrationChanged();
}

void Backend::calibrationStep() {
    if (calPlayer_ < 0) return;
    if (calStep_ == QStringLiteral("intro")) {
        calStep_ = QStringLiteral("centre");
        calRecorder_.beginCentre();
        calClock_.start();
        calTimer_.start();
    } else if (calStep_ == QStringLiteral("adjust")) {
        calStep_ = QStringLiteral("test");
        input_.setCalibration(calPlayer_, calResult_);
    }
    emit calibrationChanged();
}

void Backend::calibrationTick() {
    if (calPlayer_ < 0) {
        calTimer_.stop();
        return;
    }
    const auto l = input_.live(calPlayer_);
    calRecorder_.add(l.nx, l.ny);
    const double t = static_cast<double>(calClock_.elapsed()) / 1000.0;
    if (calStep_ == QStringLiteral("centre") && t >= 3.0) {
        calStep_ = QStringLiteral("range");
        calRecorder_.beginRange();
        calClock_.start();
    } else if (calStep_ == QStringLiteral("range") && t >= 8.0) {
        calTimer_.stop();
        calResult_ = calRecorder_.result(calResult_.deadzone);
        calStep_ = calResult_.digital ? QStringLiteral("test") : QStringLiteral("adjust");
        input_.setCalibration(calPlayer_, calResult_);
    }
    emit calibrationChanged();
}

void Backend::calibrationDeadzone(int delta) {
    if (calPlayer_ < 0 || calResult_.digital) return;
    calResult_.deadzone = std::clamp(calResult_.deadzone + delta * 0.05, 0.1, 0.85);
    input_.setCalibration(calPlayer_, calResult_);
    emit calibrationChanged();
}

void Backend::calibrationSave() {
    if (calPlayer_ < 0) return;
    const auto l = input_.live(calPlayer_);
    std::string err;
    if (zen1::input::saveCalibration(controlsFile_.toStdString(), l.calibrationKey, calResult_, &err))
        setMessage(calResult_.digital
                       ? QStringLiteral("Player %1: digital stick, no correction needed. Saved.").arg(calPlayer_ + 1)
                       : QStringLiteral("Player %1 joystick calibration saved for this controller.").arg(calPlayer_ + 1));
    else
        setMessage(QStringLiteral("Could not save: ") + QString::fromStdString(err));
    input_.setStickSuppressed(calPlayer_, false);
    calPlayer_ = -1;
    calStep_.clear();
    emit calibrationChanged();
}

void Backend::calibrationCancel() {
    calTimer_.stop();
    if (calPlayer_ >= 0) {
        const auto l = input_.live(calPlayer_);
        input_.setCalibration(calPlayer_, zen1::input::loadCalibration(controlsFile_.toStdString(), l.calibrationKey));
        input_.setStickSuppressed(calPlayer_, false);
    }
    calPlayer_ = -1;
    calStep_.clear();
    emit calibrationChanged();
}

void Backend::calibrationReset(int player) {
    const auto l = input_.live(player);
    if (!l.connected) {
        setMessage(QStringLiteral("Player %1 has no panel connected.").arg(player + 1));
        return;
    }
    zen1::input::resetCalibration(controlsFile_.toStdString(), l.calibrationKey);
    input_.setCalibration(player, {});
    setMessage(QStringLiteral("Player %1 joystick back to default settings.").arg(player + 1));
}
