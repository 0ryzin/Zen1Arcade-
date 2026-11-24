// SPDX-License-Identifier: GPL-3.0-or-later
//
// zen1-arcade: the Zen1 Arcade frontend. Phase 2 shell: opens fullscreen on
// the cabinet screen, prepares ~/Zen1Arcade, shows Player 1/2 status and
// launches the engine test card.

#include "backend.hpp"

#include "zen1/data_root.hpp"
#include "zen1/settings.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QImage>
#include <QScreen>
#include <QTimer>

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

}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Zen1 Arcade"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    Backend backend;
    QString error;
    const bool ok = backend.init(&error);

    zen1::Settings display;
    display.load(zen1::DataPaths{zen1::defaultDataRoot()}.displayFile());
    QScreen* screen = chooseScreen(QString::fromStdString(display.getString("cabinet.qt_screen")));
    backend.setScreenInfo(QStringLiteral("%1  %2 x %3")
                              .arg(screen->name())
                              .arg(screen->size().width())
                              .arg(screen->size().height()));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("zen1"), &backend);
    engine.rootContext()->setContextProperty(QStringLiteral("startupError"), ok ? QString() : error);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) return 1;
    window->setScreen(screen);
    window->setGeometry(screen->geometry());
    window->showFullScreen();

    QObject::connect(&backend, &Backend::quitRequested, &app, &QGuiApplication::quit);

    // Automated check: zen1-arcade --screenshot out.png  (renders, saves, quits)
    const QStringList args = QGuiApplication::arguments();
    const qsizetype shotArg = args.indexOf(QStringLiteral("--screenshot"));
    if (shotArg > 0 && shotArg + 1 < args.size()) {
        const QString path = args.at(shotArg + 1);
        QTimer::singleShot(1500, &app, [window, path] {
            const bool saved = window->grabWindow().save(path);
            QGuiApplication::exit(saved ? 0 : 3);
        });
    }
    return app.exec();
}
