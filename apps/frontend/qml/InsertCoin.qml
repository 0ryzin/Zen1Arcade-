// SPDX-License-Identifier: GPL-3.0-or-later
//
// INSERT COIN: waits, for as long as it takes, for a player to press a
// button. Then the coin drops (sound), the credit counter ticks to 01,
// "PLAYER 1 START" flashes, and the library opens.
import QtQuick
import Zen1 1.0

Item {
    id: ic
    signal done()
    property real clock: 0          // seconds, drives every animation here
    property int credits: 0
    property bool starting: false
    property bool active: visible
    readonly property var titles: {
        let t = []
        for (let i = 0; i < zen1.games.length && t.length < 24; ++i)
            if (zen1.games[i].status !== "unsupported") t.push(zen1.games[i].title.toUpperCase())
        return t
    }

    function handle(name) {
        if (starting) return
        starting = true
        credits = 1
        zen1.sfx("coin")
        flashAnim.restart()
        startTimer.start()
    }
    function reset() { starting = false; credits = 0; flash.opacity = 0 }

    NumberAnimation on clock {
        running: ic.active
        from: 0; to: 100000; duration: 100000 * 1000
        loops: Animation.Infinite
    }
    Timer {  // attract-mode bleep now and then (quiet; can be turned off)
        interval: 7000; running: ic.active && !ic.starting; repeat: true
        onTriggered: zen1.sfx("ambience")
    }
    Timer {
        id: startTimer
        interval: 1300
        onTriggered: ic.done()
    }

    Backdrop {
        anchors.fill: parent
        horizon: 0.62
        phase: (ic.clock / 3) % 1
        lineOpacity: ic.starting ? 1.0 : 0.8
    }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.25 }

    // Twinkling pixel stars
    Repeater {
        model: 60
        Rectangle {
            readonly property real sx: (index * 7919 % 1000) / 1000
            readonly property real sy: (index * 104729 % 997) / 997
            x: ic.width * sx
            y: ic.height * 0.58 * sy
            width: Math.max(2, Theme.px(index % 5 === 0 ? 4 : 2)); height: width
            color: index % 4 === 0 ? Theme.cyan : index % 4 === 1 ? Theme.pink : "white"
            opacity: 0.25 + 0.75 * Math.abs(Math.sin(ic.clock * (0.7 + sx) + index))
        }
    }

    // Logo
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.12
        spacing: Theme.px(18)
        NeonText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "ZEN1"
            size: Theme.px(112)
            color: "#eafcff"; glowColor: Theme.cyan
            glow: 0.9 + 0.2 * Math.sin(ic.clock * 2)
        }
        NeonText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "ARCADE"
            size: Theme.px(40); spacing: Theme.px(16)
            color: "#ffe6fa"; glowColor: Theme.pink
        }
    }

    // Spinning pixel coins
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.40
        spacing: Theme.px(70)
        Repeater {
            model: 3
            Item {
                width: Theme.px(56); height: width
                readonly property real turn: Math.abs(Math.cos(ic.clock * 2.4 + index * 1.1))
                y: -Math.abs(Math.sin(ic.clock * 2.4 + index * 1.1)) * Theme.px(14)
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * Math.max(0.12, parent.turn); height: parent.height
                    radius: Theme.px(4)
                    color: Theme.amber
                    border.color: "#a86a00"; border.width: Theme.px(4)
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.3; height: parent.height * 0.5
                        color: "#a86a00"
                        visible: parent.width > Theme.px(18)
                    }
                }
            }
        }
    }

    // INSERT COIN
    NeonText {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.53
        text: ic.starting ? "PLAYER 1 START" : "INSERT COIN"
        size: Theme.px(56)
        spacing: Theme.px(6)
        color: ic.starting ? "#eafcff" : Theme.amber
        glowColor: ic.starting ? Theme.cyan : "#ff8a00"
        opacity: ic.starting ? (Math.floor(ic.clock * 10) % 2 ? 1 : 0.4)
                             : ((ic.clock % 1.0) < 0.62 ? 1 : 0.08)
    }
    PixelText {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.62
        text: "PRESS ANY BUTTON"
        size: 16
        color: Theme.dim
        visible: !ic.starting
        opacity: 0.55 + 0.45 * Math.sin(ic.clock * 3)
    }

    // Credit counter and free play, like every cabinet's bottom line
    PixelText {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        anchors.margins: Theme.px(36)
        text: "CREDIT " + (ic.credits < 10 ? "0" : "") + ic.credits
        size: 24
        color: ic.credits > 0 ? Theme.green : Theme.text
    }
    PixelText {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: Theme.px(36)
        text: "FREE PLAY"
        size: 24
        color: Theme.pink
    }

    // Attract ticker: the games on this cabinet
    Item {
        anchors.left: parent.left; anchors.right: parent.right
        y: parent.height * 0.86
        height: Theme.px(28)
        clip: true
        visible: ic.titles.length > 0
        PixelText {
            id: ticker
            text: "NOW ON THIS CABINET  *  " + ic.titles.join("  *  ") + "  *  "
            size: 16
            color: Theme.cyan
            x: parent.width - ((ic.clock * Theme.px(120)) % (implicitWidth + parent.width))
        }
    }

    // Scanlines and the coin flash
    Column {
        anchors.fill: parent
        opacity: 0.16
        spacing: Math.max(1, Theme.px(3))
        Repeater {
            model: Math.ceil(ic.height / Math.max(2, Theme.px(4)))
            Rectangle { width: ic.width; height: Math.max(1, Theme.u); color: "black" }
        }
    }
    Rectangle {
        id: flash
        anchors.fill: parent
        color: "white"
        opacity: 0
        NumberAnimation on opacity { id: flashAnim; running: false; from: 0.7; to: 0; duration: 600; easing.type: Easing.OutCubic }
    }
}
