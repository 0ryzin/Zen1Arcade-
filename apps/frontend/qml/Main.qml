// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zen1 Arcade frontend: intro -> library -> game (engine) -> back to the
// library. Settings and the controller screen hang off the library.
import QtQuick
import QtQuick.Window
import Zen1 1.0

Window {
    id: root
    visible: false          // shown fullscreen from C++ on the cabinet screen
    color: "black"
    title: "Zen1 Arcade"

    property string page: "boot"   // boot | intro | library | settings | controls
    property real introTime: 0
    readonly property bool externalClock: launchOptions.recording || launchOptions.introTime >= 0
    readonly property real introDuration: (zen1.settings.introSeconds || 6) * 1000

    function updateScale() { Theme.u = Math.max(0.3, Math.min(width / 1280, height / 1024)) }
    onWidthChanged: updateScale()
    onHeightChanged: updateScale()

    Component.onCompleted: {
        updateScale()
        if (launchOptions.page !== "" && launchOptions.page !== "intro") page = launchOptions.page
        else if (launchOptions.page === "intro") startIntro()
        else if (launchOptions.skipIntro || !zen1.settings.introEnabled) page = "library"
        else startIntro()
        if (launchOptions.introTime >= 0) introTime = launchOptions.introTime
    }

    function startIntro() {
        intro.done = false
        introTime = 0
        page = "intro"
        if (!externalClock) {
            introClock.restart()
            if (zen1.settings.introSound) zen1.playIntroSound(introDuration / 1000)
        }
    }
    function finishIntro(skipped) {
        introClock.stop()
        if (skipped) zen1.stopIntroSound()
        page = "library"
    }
    function input(name) {
        if (page === "intro") {
            if (zen1.settings.introSkippable && !externalClock) finishIntro(true)
            return
        }
        if (zen1.engineRunning) return
        if (page === "library") library.handle(name)
        else if (page === "settings") settings.handle(name)
        else if (page === "controls") controls.handle(name)
    }

    NumberAnimation {
        id: introClock
        target: root
        property: "introTime"
        from: 0
        to: root.introDuration
        duration: root.introDuration
    }

    Connections {
        target: zen1
        function onControl(player, name) { if (player === 0) root.input(name) }
        function onGameExited() { root.requestActivate() }
    }

    Item {
        id: keys
        anchors.fill: parent
        focus: true
        // Keyboard (development, and the Steam Deck in Desktop Mode, whose
        // controls type arrow keys / Enter / Escape there).
        Keys.onPressed: (event) => {
            const map = {}
            map[Qt.Key_Up] = "up"; map[Qt.Key_Down] = "down"; map[Qt.Key_Left] = "left"; map[Qt.Key_Right] = "right"
            map[Qt.Key_W] = "up"; map[Qt.Key_S] = "down"; map[Qt.Key_A] = "left"; map[Qt.Key_D] = "right"
            map[Qt.Key_Return] = "lp"; map[Qt.Key_Enter] = "lp"; map[Qt.Key_Space] = "lp"
            map[Qt.Key_Escape] = "lk"; map[Qt.Key_Backspace] = "lk"
            map[Qt.Key_F] = "mp"; map[Qt.Key_Slash] = "hp"; map[Qt.Key_Question] = "hp"
            map[Qt.Key_PageUp] = "mk"; map[Qt.Key_Q] = "mk"; map[Qt.Key_PageDown] = "hk"; map[Qt.Key_E] = "hk"
            map[Qt.Key_1] = "player"
            if (zen1.captureActive && event.key === Qt.Key_Escape) { zen1.cancelCapture(); event.accepted = true; return }
            const name = map[event.key]
            if (name) { root.input(name); event.accepted = true }
            else if (root.page === "intro") root.input("lp")
        }
    }

    Library {
        id: library
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "library" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 450; easing.type: Easing.OutCubic } }
        onOpenSettings: { settings.sel = 0; root.page = "settings" }
    }
    SettingsPage {
        id: settings
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "settings" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 250 } }
        onBack: root.page = "library"
        onReplayIntro: root.startIntro()
        onOpenControls: { controls.sel = 0; root.page = "controls" }
    }
    ControlsPage {
        id: controls
        anchors.fill: parent
        visible: opacity > 0
        opacity: root.page === "controls" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 250 } }
        onBack: root.page = "settings"
    }
    Intro {
        id: intro
        anchors.fill: parent
        visible: root.page === "intro"
        time: root.introTime
        duration: root.introDuration
        onFinished: if (!root.externalClock) root.finishIntro(false)
    }

    // Message toast
    Rectangle {
        id: toast
        visible: zen1.message !== "" && root.page !== "intro" && root.page !== "boot"
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.70
        width: Math.min(parent.width * 0.85, toastText.implicitWidth + Theme.px(60))
        height: toastText.implicitHeight + Theme.px(36)
        radius: Theme.px(14)
        color: Qt.rgba(0.05, 0.02, 0.12, 0.95)
        border.color: Theme.amber
        border.width: Math.max(2, Theme.px(2))
        Text {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, root.width * 0.85 - Theme.px(60))
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: zen1.message
            color: Theme.text
            font.pixelSize: Theme.px(22)
            font.bold: true
        }
        Timer { interval: 9000; running: toast.visible && !zen1.engineRunning; onTriggered: zen1.clearMessage() }
    }

    // While a game runs the engine has its own fullscreen window; this shows
    // during the hand-over and if the engine window is ever behind.
    Rectangle {
        anchors.fill: parent
        color: "black"
        visible: opacity > 0
        opacity: zen1.engineRunning ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 350 } }
        Column {
            anchors.centerIn: parent
            spacing: Theme.px(18)
            NeonText { anchors.horizontalCenter: parent.horizontalCenter; text: "LOADING"; size: Theme.px(60); color: Theme.text; glowColor: Theme.cyan; spacing: Theme.px(12) }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: zen1.runningTitle; color: Theme.pink; font.pixelSize: Theme.px(30); font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Hold both player buttons to come back here"; color: Theme.dim; font.pixelSize: Theme.px(20) }
        }
    }

    // Leaving Zen1 Arcade: both player buttons held
    Rectangle {
        visible: zen1.exitProgress > 0.02
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: parent.width * 0.6
        height: Theme.px(110)
        radius: Theme.px(14)
        color: Qt.rgba(0.02, 0.01, 0.08, 0.95)
        border.color: Theme.pink
        border.width: Math.max(2, Theme.px(2))
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: Theme.px(16); text: "KEEP HOLDING TO LEAVE ZEN1 ARCADE"; color: Theme.text; font.pixelSize: Theme.px(24); font.bold: true }
        Rectangle {
            x: Theme.px(30); y: Theme.px(66)
            width: (parent.width - Theme.px(60)) * Math.min(1, zen1.exitProgress)
            height: Theme.px(18); radius: height / 2
            color: Theme.pink
        }
    }

    Text {
        visible: startupError !== ""
        anchors.centerIn: parent
        text: "Data folder problem: " + startupError
        color: Theme.red
        font.pixelSize: Theme.px(26)
    }
}
