// SPDX-License-Identifier: GPL-3.0-or-later
//
// zen1-arcade: the Zen1 Arcade frontend. Opens fullscreen on the cabinet
// screen (an external 1280x1024 screen is preferred over the Deck's own),
// plays the intro, shows the library and runs games through zen1-engine.
//
// Automated checks (used by the build server):
//   --screenshot FILE [--page intro|library|settings|controls] [--intro-time MS]
//   --record-intro DIR [--fps N]   every intro frame as PNG + intro.wav
//   --demo-library                 fixed game list instead of the Games folder
//   --windowed --size WxH          window instead of fullscreen

#include "backend.hpp"
#include "intro_audio.hpp"

#include "zen1/data_root.hpp"
#include "zen1/settings.hpp"
#include "zen1/version.hpp"

#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QtQml>

#include <csignal>
#include <cstdio>
#include <execinfo.h>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <memory>

namespace {

// Saved choice, else a 1280x1024 screen, else the primary screen.
QScreen* chooseScreen(const QString& saved) {
    const auto screens = QGuiApplication::screens();
    for (QScreen* s : screens)
        if (!saved.isEmpty() && s->name() == saved) return s;
    for (QScreen* s : screens)
        if (s->size() == QSize(1280, 1024)) return s;
    return QGuiApplication::primaryScreen();
}

QImage grab(QQuickWindow* w) {
    QImage img = w->grabWindow();
    if (img.isNull() && w->screen()) img = w->screen()->grabWindow(w->winId()).toImage();
    return img;
}

QString argValue(const QStringList& args, const QString& name, const QString& def = {}) {
    const qsizetype i = args.indexOf(name);
    return i > 0 && i + 1 < args.size() ? args.at(i + 1) : def;
}

}  // namespace

namespace {
// If the frontend itself ever crashes, the reason and a backtrace go to
// Logs/zen1-arcade-crash.txt (Game Mode shows no terminal output).
int g_crashFd = -1;
void onCrash(int sig) {
    const char* name = sig == SIGSEGV ? "SIGSEGV" : sig == SIGFPE ? "SIGFPE" : sig == SIGBUS ? "SIGBUS"
                     : sig == SIGILL ? "SIGILL" : sig == SIGABRT ? "SIGABRT" : "signal";
    char msg[96];
    const int n = std::snprintf(msg, sizeof msg, "zen1-arcade: crashed (%s); backtrace:\n", name);
    void* frames[64];
    const int count = ::backtrace(frames, 64);
    for (int fd : {2, g_crashFd}) {
        if (fd < 0) continue;
        if (n > 0) (void)!::write(fd, msg, static_cast<size_t>(n));
        ::backtrace_symbols_fd(frames, count, fd);
    }
    std::signal(sig, SIG_DFL);
    ::raise(sig);
}
void installCrashLog() {
    const auto logs = zen1::DataPaths{zen1::defaultDataRoot()}.root / "Logs";
    std::error_code ec;
    std::filesystem::create_directories(logs, ec);
    g_crashFd = ::open((logs / "zen1-arcade-crash.txt").c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    for (int sig : {SIGSEGV, SIGFPE, SIGBUS, SIGILL, SIGABRT}) std::signal(sig, onCrash);
}
}  // namespace

int main(int argc, char* argv[]) {
    installCrashLog();
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Zen1 Arcade"));
    QGuiApplication::setApplicationVersion(QString::fromStdString(zen1::kVersion));
    const QStringList args = QGuiApplication::arguments();

    const QString shotPath = argValue(args, QStringLiteral("--screenshot"));
    const QString recordDir = argValue(args, QStringLiteral("--record-intro"));
    const int fps = argValue(args, QStringLiteral("--fps"), QStringLiteral("30")).toInt();
    const bool windowed = args.contains(QStringLiteral("--windowed"));
    QSize size;
    {
        const QStringList wh = argValue(args, QStringLiteral("--size")).split(QLatin1Char('x'));
        if (wh.size() == 2) size = QSize(wh[0].toInt(), wh[1].toInt());
    }

    // Pixel fonts (SIL Open Font License, see fonts/OFL-*.txt).
    for (const char* f : {":/fonts/PressStart2P-Regular.ttf", ":/fonts/VT323-Regular.ttf", ":/fonts/PixelifySans.ttf"})
        if (QFontDatabase::addApplicationFont(QString::fromLatin1(f)) < 0) std::fprintf(stderr, "font %s not loaded\n", f);
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/qml/Theme.qml")), "Zen1", 1, 0, "Theme");

    Backend backend;
    QString error;
    const bool ok = backend.init(&error);
    if (args.contains(QStringLiteral("--demo-library"))) backend.useDemoLibrary();

    zen1::Settings display;
    display.load(zen1::DataPaths{zen1::defaultDataRoot()}.displayFile());
    QScreen* screen = chooseScreen(QString::fromStdString(display.getString("cabinet.qt_screen")));
    backend.setScreenInfo(QStringLiteral("%1  %2 x %3").arg(screen->name()).arg(screen->size().width()).arg(screen->size().height()));

    QVariantMap options;
    options[QStringLiteral("page")] = recordDir.isEmpty() ? argValue(args, QStringLiteral("--page")) : QStringLiteral("intro");
    options[QStringLiteral("recording")] = !recordDir.isEmpty();
    options[QStringLiteral("skipIntro")] = args.contains(QStringLiteral("--skip-intro"));
    options[QStringLiteral("introTime")] = argValue(args, QStringLiteral("--intro-time"), QStringLiteral("-1")).toDouble();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("zen1"), &backend);
    engine.rootContext()->setContextProperty(QStringLiteral("startupError"), ok ? QString() : error);
    engine.rootContext()->setContextProperty(QStringLiteral("launchOptions"), options);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) return 1;
    window->setScreen(screen);
    if (windowed || !recordDir.isEmpty()) {
        window->resize(size.isValid() ? size : QSize(1280, 1024));
        window->show();
    } else {
        window->setGeometry(screen->geometry());
        window->showFullScreen();
    }
    QObject::connect(&backend, &Backend::quitRequested, &app, &QGuiApplication::quit);
    // While a game runs the frontend stays open, black, under the game's
    // own fullscreen window. It used to hide itself; under the Steam Deck's
    // Game Mode compositor a hidden window is not always shown again, which
    // looks exactly like Zen1 closing when a game ends. Kept open, the
    // library is simply there again when the game's window goes away.
    const bool fullscreen = !(windowed || !recordDir.isEmpty());
    QObject::connect(&backend, &Backend::gameExited, window, [window, fullscreen] {
        if (fullscreen) window->showFullScreen();
        else window->show();
        window->requestActivate();
    });

    // End-to-end assignment test (CI): start "Assign Player N" as the
    // Controls page does, then report how it ended (a test script presses a
    // button on a controller meanwhile).
    const QString testAssign = argValue(args, QStringLiteral("--test-assign"));
    if (!testAssign.isEmpty()) {
        const int player = testAssign.toInt() - 1;
        QObject::connect(&backend, &Backend::captureChanged, &app, [&app, &backend] {
            std::fprintf(stderr, "test-assign: %s | prompt: %s\n", backend.captureActive() ? "waiting" : "finished",
                         qPrintable(QString(backend.capturePrompt()).replace(QLatin1Char('\n'), QLatin1Char(' '))));
            if (!backend.captureActive()) {
                std::fprintf(stderr, "test-assign: result: %s\n", qPrintable(backend.message()));
                QTimer::singleShot(500, &app, [] { QGuiApplication::exit(0); });
            }
        });
        QTimer::singleShot(2500, &app, [&backend, player] {
            std::fprintf(stderr, "test-assign: assigning player %d\n", player + 1);
            backend.startAssign(player);
        });
    }

    // End-to-end launch test (CI and diagnostics): start SET from the
    // library as a player would, and report what happened to the frontend.
    //   exit 0: the game ran and the frontend came back (or, for a game that
    //           cannot start, the frontend stayed and showed why)
    //   exit 7: the frontend was about to quit while a game was running
    const QString testLaunch = argValue(args, QStringLiteral("--test-launch"));
    if (!testLaunch.isEmpty()) {
        auto started = std::make_shared<bool>(false);
        auto returned = std::make_shared<bool>(false);
        QObject::connect(&backend, &Backend::gameStarted, &app, [started] {
            *started = true;
            std::fprintf(stderr, "test-launch: engine started\n");
        });
        QObject::connect(&backend, &Backend::engineReady, &app,
                         [] { std::fprintf(stderr, "test-launch: engine ready (game window up)\n"); });
        QObject::connect(&backend, &Backend::gameExited, &app, [&app, returned, &backend, window] {
            *returned = true;
            std::fprintf(stderr, "test-launch: back in the library (window visible: %d) message: %s\n",
                         window->isVisible() ? 1 : 0, qPrintable(backend.message()));
            QTimer::singleShot(1500, &app, [] { QGuiApplication::exit(0); });
        });
        QObject::connect(&app, &QGuiApplication::aboutToQuit, &app, [started, returned] {
            if (*started && !*returned) {
                std::fprintf(stderr, "test-launch: FRONTEND QUIT WHILE THE GAME WAS RUNNING\n");
                std::fflush(stderr);
                std::_Exit(7);
            }
        });
        if (args.contains(QStringLiteral("--test-launch-press"))) {
            // The game is only selected; the player starts it with the panel:
            // the Player 1 button inserts the coin, pressed again it launches
            // the selected game - the library's own path, inside the input handler.
            backend.setSelectedSet(testLaunch);
            QObject::connect(&backend, &Backend::control, &app, [](int player, const QString& name) {
                std::fprintf(stderr, "test-launch: press P%d %s\n", player + 1, qPrintable(name));
            });
            QTimer::singleShot(40000, &app, [started, &backend] {
                if (*started) return;
                std::fprintf(stderr, "test-launch: the panel presses did not start a game; message: %s\n",
                             qPrintable(backend.message()));
                QGuiApplication::exit(6);
            });
        } else {
        QTimer::singleShot(2500, &app, [&backend, testLaunch] {
            std::fprintf(stderr, "test-launch: launching %s\n", qPrintable(testLaunch));
            backend.launchGame(testLaunch);
        });
        QTimer::singleShot(6000, &app, [started, &backend] {
            if (*started) return;  // the game is running: wait for it to end
            std::fprintf(stderr, "test-launch: no engine started; message: %s\n", qPrintable(backend.message()));
            QGuiApplication::exit(backend.message().isEmpty() ? 5 : 0);
        });
        }
    }

    if (!recordDir.isEmpty()) {
        // Step the intro clock frame by frame and save each frame.
        QDir().mkpath(recordDir);
        const double seconds = backend.settings().value(QStringLiteral("introSeconds")).toDouble();
        const auto pcm = synthIntroAudio(48000, seconds);
        writeWav((recordDir + QStringLiteral("/intro.wav")).toStdString(), pcm, 48000);
        const int frames = static_cast<int>(seconds * fps) + 1;
        auto frame = std::make_shared<int>(0);
        auto* timer = new QTimer(&app);
        timer->setInterval(60);
        window->setProperty("introTime", 0.0);
        QObject::connect(timer, &QTimer::timeout, &app, [=] {
            const QImage img = grab(window);
            if (img.isNull()) {
                std::fprintf(stderr, "record: frame grab failed\n");
                QGuiApplication::exit(3);
                return;
            }
            img.save(recordDir + QStringLiteral("/frame_%1.png").arg(*frame, 4, 10, QLatin1Char('0')));
            if (++*frame >= frames) {
                std::fprintf(stderr, "record: %d frames at %d fps saved to %s\n", frames, fps, qPrintable(recordDir));
                timer->stop();
                QGuiApplication::exit(0);
                return;
            }
            window->setProperty("introTime", *frame * 1000.0 / fps);
        });
        timer->start();
    } else if (!shotPath.isEmpty()) {
        auto attempts = std::make_shared<int>(0);
        auto* timer = new QTimer(&app);
        timer->setInterval(700);
        QObject::connect(timer, &QTimer::timeout, &app, [=] {
            ++*attempts;
            const QImage img = grab(window);
            if (!img.isNull() && img.save(shotPath)) {
                std::fprintf(stderr, "screenshot %dx%d saved to %s\n", img.width(), img.height(), qPrintable(shotPath));
                timer->stop();
                QGuiApplication::exit(0);
            } else if (*attempts >= 10) {
                std::fprintf(stderr, "screenshot failed\n");
                timer->stop();
                QGuiApplication::exit(3);
            }
        });
        timer->start();
    }
    return app.exec();
}
