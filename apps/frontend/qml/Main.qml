// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zen1 Arcade frontend: start-up scene (intro settling into INSERT COIN) -> library ->
// game (engine) -> back to the same game in the library. SETUP and the
// controller screens hang off the library.
import QtQuick
import QtQuick.Window
import Zen1 1.0

Window {
    id: root
    visible: false          // shown fullscreen from C++ on the cabinet screen
    color: "black"
    title: "Zen1 Arcade"

    // boot | attract | library | settings | controls | buttons | test | calibrate
    property string page: "boot"
    // The intro part of the start-up scene is playing (its own sound, no music).
    property bool introPlaying: false
    // Menu music on every screen except while the intro plays.
    readonly property bool musicScene: page !== "boot" && !(page === "attract" && introPlaying)
    onMusicSceneChanged: zen1.setMusicScene(musicScene)
    property real introTime: 0
    readonly property bool externalClock: launchOptions.recording || launchOptions.introTime >= 0
    readonly property real introDuration: (zen1.settings.introSeconds || 6) * 1000

    function updateScale() { Theme.u = Math.max(0.3, Math.min(width / 1280, height / 1024)) }
    onWidthChanged: updateScale()
    onHeightChanged: updateScale()

    Component.onCompleted: {
        updateScale()
        const p = launchOptions.page
        if (p === "intro") startIntro()
        else if (p === "coin") { startIntro(); settle() }
        else if (p === "calibrate") { page = "calibrate"; calibrate.player = 0 }
        else if (p !== "") page = p
        else if (launchOptions.skipIntro || !zen1.settings.introEnabled) afterIntro()
        else startIntro()
        if (launchOptions.introTime >= 0) introTime = launchOptions.introTime
    }

    function startIntro() {
        idleClock.stop()
        scene.reset()
        introTime = 0
        introPlaying = true
        page = "attract"
        if (!externalClock) {
            introClock.restart()
            if (zen1.settings.introSound) zen1.playIntroSound(introDuration / 1000)
        }
    }
    // No intro: straight to the settled INSERT COIN state, or the library.
    function afterIntro() {
        if (zen1.settings.insertCoin) { startIntro(); settle() }
        else page = "library"
    }
    // Jump the scene to its settled idle state (a skip, or no intro wanted).
    function settle() {
        introClock.stop()
        zen1.stopIntroSound()
        introPlaying = false
        introTime = introDuration
        if (!externalClock) idleClock.restart()
    }
    // The intro part has played out: the scene stays as it is and idles.
    function introDone() {
        introPlaying = false
        if (!zen1.settings.insertCoin) page = "library"
        else if (!externalClock) idleClock.restart()
    }
    function input(name) {
        if (page === "attract") {
            if (introPlaying) {
                if (!zen1.settings.introSkippable || externalClock) return
                // One press skips the intro and is also the coin.
                settle()
                if (!zen1.settings.insertCoin) { page = "library"; return }
            }
            scene.handle(name)
            return
        }
        if (zen1.engineRunning) return
        if (page === "library") library.handle(name)
        else if (page === "settings") settings.handle(name)
        else if (page === "controls") controls.handle(name)
        else if (page === "buttons") buttons.handle(name)
        else if (page === "artwork") artworkPage.handle(name)
        else if (page === "test") test.handle(name)
        else if (page === "calibrate") calibrate.handle(name)
    }

    NumberAnimation {
        id: introClock
        target: root; property: "introTime"
        from: 0; to: root.introDuration; duration: root.introDuration
    }
    // Idle time after the intro; the scene's idle animations all repeat
    // within 600 s, so the wrap is seamless.
    NumberAnimation {
        id: idleClock
        target: root; property: "introTime"
        from: root.introDuration; to: root.introDuration + 600000; duration: 600000
        loops: Animation.Infinite
    }

    Connections {
        target: zen1
        function onControl(player, name) { if (player === 0) root.input(name) }
        function onGameExited() { root.requestActivate(); library.restoreSelection() }
    }

    Item {
        id: keys
        anchors.fill: parent
        focus: true
        // Keyboard: development, and the Steam Deck in Desktop Mode (its
        // controls type arrow keys / Enter / Escape there).
        Keys.onPressed: (event) => {
            if (zen1.captureActive && event.key === Qt.Key_Escape) { zen1.cancelCapture(); event.accepted = true; return }
            if (root.page === "test" && event.key === Qt.Key_Escape) { root.page = "controls"; event.accepted = true; return }
            const map = {}
            map[Qt.Key_Up] = "up"; map[Qt.Key_Down] = "down"; map[Qt.Key_Left] = "left"; map[Qt.Key_Right] = "right"
            map[Qt.Key_W] = "up"; map[Qt.Key_S] = "down"; map[Qt.Key_A] = "left"; map[Qt.Key_D] = "right"
            map[Qt.Key_Return] = "lp"; map[Qt.Key_Enter] = "lp"; map[Qt.Key_Space] = "lp"
            map[Qt.Key_Escape] = "lk"; map[Qt.Key_Backspace] = "lk"; map[Qt.Key_Delete] = "mp"
            map[Qt.Key_F] = "mp"; map[Qt.Key_H] = "hp"
            map[Qt.Key_Q] = "mk"; map[Qt.Key_E] = "hk"
            map[Qt.Key_1] = "player"
            if (event.key === Qt.Key_Minus) { zen1.volumeStep(-1); event.accepted = true; return }
            if (event.key === Qt.Key_Equal || event.key === Qt.Key_Plus) { zen1.volumeStep(1); event.accepted = true; return }
            const name = map[event.key]
            if (name) { root.input(name); event.accepted = true }
            else if (root.page === "attract") root.input("lp")
        }
    }

    Library {
        id: library
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "library" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
        onOpenSettings: { settings.sel = 1; root.page = "settings" }
    }
    SettingsPage {
        id: settings
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "settings" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        onBack: root.page = "library"
        onReplayIntro: root.startIntro()
        onOpenControls: { controls.sel = 1; root.page = "controls" }
        onOpenTest: root.page = "test"
        onOpenArtwork: (set, title) => { artworkPage.open(set, title); root.page = "artwork" }
    }
    ArtworkPage {
        id: artworkPage
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "artwork" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        onBack: root.page = "settings"
    }
    ControlsPage {
        id: controls
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "controls" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        onBack: root.page = "settings"
        onOpenTest: root.page = "test"
        onOpenCalibrate: (player) => { calibrate.start(player); root.page = "calibrate" }
        onOpenButtons: (player) => { buttons.player = player; buttons.sel = 1; root.page = "buttons" }
    }
    ButtonsPage {
        id: buttons
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "buttons" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        onBack: root.page = "controls"
    }
    TestPage {
        id: test
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "test" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        onBack: root.page = "controls"
    }
    CalibratePage {
        id: calibrate
        anchors.fill: parent
        visible: root.page === "calibrate"
        onBack: if (root.page === "calibrate") root.page = "controls"
    }
    // Start-up scene: the intro, settling into INSERT COIN (one scene)
    Attract {
        id: scene
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "attract" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.InCubic } }
        time: root.introTime
        duration: root.introDuration
        coinEnabled: zen1.settings.insertCoin
        onIntroFinished: if (!root.externalClock && root.introPlaying) root.introDone()
        onDone: root.page = "library"
    }

    // Message toast
    Rectangle {
        id: toast
        visible: zen1.message !== "" && root.page !== "attract" && root.page !== "boot"
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.42
        width: Math.min(parent.width * 0.86, toastText.implicitWidth + Theme.px(60))
        height: toastText.implicitHeight + Theme.px(40)
        color: Qt.rgba(0.04, 0.02, 0.1, 0.96)
        border.color: Theme.amber
        border.width: Math.max(3, Theme.px(3))
        Text {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, root.width * 0.86 - Theme.px(60))
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: zen1.message
            font.family: Theme.mono
            font.pixelSize: Theme.px(32)
            color: Theme.text
        }
        Timer { interval: 7000; running: toast.visible && !zen1.engineRunning; onTriggered: zen1.clearMessage() }
    }

    // Hand-over to the engine (also shown if its window is ever behind)
    Rectangle {
        anchors.fill: parent
        color: "black"
        visible: opacity > 0
        opacity: zen1.engineRunning ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 300 } }
        Column {
            anchors.centerIn: parent
            spacing: Theme.px(22)
            NeonText { anchors.horizontalCenter: parent.horizontalCenter; text: "LOADING"; size: Theme.px(48); color: Theme.text; glowColor: Theme.cyan }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: zen1.runningTitle; font.family: Theme.title; font.pixelSize: Theme.px(36); color: Theme.pink }
            PixelText { anchors.horizontalCenter: parent.horizontalCenter; text: "HOLD 1P 5 SEC: LIBRARY   HOLD 2P 5 SEC: RESTART   DOUBLE 1P/2P: VOLUME"; size: 8; color: Theme.dim }
        }
    }

    // Leaving Zen1 Arcade: both player buttons held in the menus
    Rectangle {
        visible: zen1.exitProgress > 0.02
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: parent.width * 0.6
        height: Theme.px(110)
        color: Qt.rgba(0.02, 0.01, 0.08, 0.95)
        border.color: Theme.pink
        border.width: Math.max(2, Theme.px(2))
        PixelText { anchors.horizontalCenter: parent.horizontalCenter; y: Theme.px(20); text: "KEEP HOLDING TO LEAVE ZEN1"; size: 16 }
        Rectangle {
            x: Theme.px(30); y: Theme.px(66)
            width: (parent.width - Theme.px(60)) * Math.min(1, zen1.exitProgress)
            height: Theme.px(18)
            color: Theme.pink
        }
    }

    VolumeOverlay { z: 50 }

    Text {
        visible: startupError !== ""
        anchors.centerIn: parent
        text: "Data folder problem: " + startupError
        color: Theme.red
        font.pixelSize: Theme.px(26)
    }
}
