// SPDX-License-Identifier: GPL-3.0-or-later
//
// Frontend state exposed to QML as `zen1`: the game library (from the
// engine's own --scan), artwork, launching games and the test card,
// settings, sound effects, and the controller system (assignment, button
// remapping, live test and joystick calibration).
#pragma once

#include "input_watcher.hpp"
#include "intro_audio.hpp"

#include "zen1/input/calibration.hpp"

#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString dataRoot READ dataRoot NOTIFY changed)
    Q_PROPERTY(QString gamesDir READ gamesDir NOTIFY changed)
    Q_PROPERTY(QString screenInfo READ screenInfo NOTIFY changed)
    Q_PROPERTY(QString player1 READ player1 NOTIFY changed)
    Q_PROPERTY(QString player2 READ player2 NOTIFY changed)
    Q_PROPERTY(bool player1Assigned READ player1Assigned NOTIFY changed)
    Q_PROPERTY(bool player2Assigned READ player2Assigned NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY changed)
    Q_PROPERTY(QString runningTitle READ runningTitle NOTIFY changed)
    Q_PROPERTY(double exitProgress READ exitProgress NOTIFY exitProgressChanged)
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(QString selectedSet READ selectedSet WRITE setSelectedSet NOTIFY selectedChanged)
    Q_PROPERTY(bool captureActive READ captureActive NOTIFY captureChanged)
    Q_PROPERTY(QString capturePrompt READ capturePrompt NOTIFY captureChanged)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList p1Map READ p1Map NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QVariantMap p1Live READ p1Live NOTIFY liveChanged)
    Q_PROPERTY(QVariantMap p2Live READ p2Live NOTIFY liveChanged)
    Q_PROPERTY(QVariantMap calibration READ calibration NOTIFY calibrationChanged)
    Q_PROPERTY(QString artworkStatus READ artworkStatus NOTIFY artworkChanged)

public:
    explicit Backend(QObject* parent = nullptr);
    ~Backend() override;

    bool init(QString* error);
    void setScreenInfo(const QString& s);
    void setEngineDir(const QString& dir) { engineDir_ = dir; }

    QString version() const;
    QString dataRoot() const { return dataRoot_; }
    QString gamesDir() const { return gamesDir_; }
    QString screenInfo() const { return screenInfo_; }
    QString player1() const { return input_.playerStatus(0); }
    QString player2() const { return input_.playerStatus(1); }
    bool player1Assigned() const { return input_.assigned(0); }
    bool player2Assigned() const { return input_.assigned(1); }
    QString message() const { return message_; }
    bool engineRunning() const { return engine_ != nullptr; }
    QString runningTitle() const { return runningTitle_; }
    double exitProgress() const { return exitProgress_; }
    QVariantList games() const { return games_; }
    QString selectedSet() const { return selectedSet_; }
    void setSelectedSet(const QString& s);
    bool captureActive() const { return captureMode_ != CaptureMode::None; }
    QString capturePrompt() const { return capturePrompt_; }
    QVariantMap settings() const;
    QVariantList p1Map() const;
    QVariantList devices() const;
    QVariantMap p1Live() const { return liveMap(0); }
    QVariantMap p2Live() const { return liveMap(1); }
    QVariantMap calibration() const;
    QString artworkStatus() const { return artworkStatus_; }

    Q_INVOKABLE void rescan();
    Q_INVOKABLE void launchGame(const QString& set);
    Q_INVOKABLE void launchTestCard();
    Q_INVOKABLE void setSetting(const QString& key, const QVariant& value);
    Q_INVOKABLE void playIntroSound(double seconds);
    Q_INVOKABLE void stopIntroSound();
    Q_INVOKABLE void sfx(const QString& name);
    Q_INVOKABLE void refreshControllers();
    Q_INVOKABLE void startAssign(int player);
    Q_INVOKABLE void startRemap(int player);
    Q_INVOKABLE void cancelCapture();
    Q_INVOKABLE void swapPlayers();
    Q_INVOKABLE void unassign(int player);
    Q_INVOKABLE void resetControls();
    Q_INVOKABLE void clearMessage() { setMessage(QString()); }
    Q_INVOKABLE void quit() { emit quitRequested(); }
    // Artwork
    Q_INVOKABLE void downloadArtwork();
    Q_INVOKABLE void nextArtwork(const QString& set);
    Q_INVOKABLE void resetArtwork(const QString& set);
    // Calibration wizard: begin(player) -> centre -> range -> adjust -> save/cancel
    Q_INVOKABLE void calibrationBegin(int player);
    Q_INVOKABLE void calibrationStep();
    Q_INVOKABLE void calibrationDeadzone(int delta);
    Q_INVOKABLE void calibrationSave();
    Q_INVOKABLE void calibrationCancel();
    Q_INVOKABLE void calibrationReset(int player);

    void useDemoLibrary();

signals:
    void changed();
    void exitProgressChanged();
    void gamesChanged();
    void selectedChanged();
    void captureChanged();
    void settingsChanged();
    void liveChanged();
    void calibrationChanged();
    void artworkChanged();
    void control(int player, const QString& name);  // menu input from the panels
    void gameStarted();
    void engineReady();
    void gameExited();
    void quitRequested();

private:
    enum class CaptureMode { None, Assign, Remap };
    void setMessage(const QString& m);
    void onEngineLine(const QString& line);
    void checkExitHold();
    void startEngine(const QStringList& args, const QString& title, const QString& set);
    void onCaptured(int deviceIndex, int buttonIndex);
    void finishCapture(const QString& msg);
    void applyArtwork();
    QStringList artworkCandidates(const QString& set) const;
    void downloadNext();
    QVariantMap liveMap(int player) const;
    void calibrationTick();

    QString dataRoot_, gamesDir_, configsDir_, controlsFile_, settingsFile_, displayFile_, libraryFile_,
        artworkDir_, engineDir_;
    QString screenInfo_, message_, runningTitle_, runningSet_, engineError_, selectedSet_;
    QVariantList games_;
    InputWatcher input_;
    IntroAudioPlayer introAudio_;
    SfxPlayer sfx_;
    QLocalServer server_;
    QPointer<QLocalSocket> link_;
    QProcess* engine_ = nullptr;
    QTimer holdTimer_;
    QElapsedTimer holdClock_;
    double exitProgress_ = 0.0;

    CaptureMode captureMode_ = CaptureMode::None;
    int capturePlayer_ = 0;
    int captureStep_ = 0;
    int captureDevice_ = -1;
    std::vector<int> captureButtons_;
    QString capturePrompt_;

    QNetworkAccessManager net_;
    QStringList downloadQueue_;  // "set|kind"
    QString artworkStatus_;

    int calPlayer_ = -1;
    QString calStep_;  // intro, centre, range, adjust
    QTimer calTimer_;
    QElapsedTimer calClock_;
    zen1::input::CalibrationRecorder calRecorder_;
    zen1::input::StickCalibration calResult_;
};
