// SPDX-License-Identifier: GPL-3.0-or-later
//
// The library: every game in the Games folder in one big grid, artwork
// first. Player 1's stick moves (with repeat for long lists), the 1P button
// or LP plays. Up from the top row reaches SETUP. The selected game's
// details and controls are in the panel below.
import QtQuick
import Zen1 1.0

Item {
    id: lib
    signal openSettings()
    property string zone: "grid"      // "grid" or "setup"
    property real pulse: 0
    readonly property var games: zen1.games
    readonly property var current: grid.currentIndex >= 0 && grid.currentIndex < games.length ? games[grid.currentIndex] : null
    readonly property int columns: 4

    function restoreSelection() {
        for (let i = 0; i < games.length; ++i)
            if (games[i].set === zen1.selectedSet) { grid.currentIndex = i; grid.positionViewAtIndex(i, GridView.Contain); return }
        if (grid.currentIndex >= games.length) grid.currentIndex = Math.max(0, games.length - 1)
    }
    onGamesChanged: restoreSelection()
    Component.onCompleted: restoreSelection()

    function move(fn) {
        const before = grid.currentIndex
        fn()
        if (grid.currentIndex !== before) {
            zen1.sfx("move")
            if (current) zen1.selectedSet = current.set
        }
    }

    function handle(name) {
        if (zone === "setup") {
            if (name === "down" || name === "lk") { zone = "grid"; zen1.sfx("move") }
            else if (name === "lp" || name === "player") { zen1.sfx("select"); openSettings() }
            return
        }
        switch (name) {
        case "left": move(function() { grid.moveCurrentIndexLeft() }); break
        case "right": move(function() { grid.moveCurrentIndexRight() }); break
        case "down": move(function() { grid.moveCurrentIndexDown() }); break
        case "up":
            if (grid.currentIndex < columns) { zone = "setup"; zen1.sfx("move") }
            else move(function() { grid.moveCurrentIndexUp() })
            break
        case "lp":
        case "player":
            if (current) {
                if (current.status !== "unsupported" && current.status === "ready") zen1.sfx("select")
                zen1.launchGame(current.set)
            }
            break
        case "lk":
            if (zen1.message !== "") zen1.clearMessage()
            break
        }
    }

    SequentialAnimation on pulse {
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: 700; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0; duration: 700; easing.type: Easing.InOutSine }
    }

    Backdrop {
        anchors.fill: parent
        horizon: 0.7
        lineOpacity: 0.45
        NumberAnimation on phase { from: 0; to: 1; duration: 6000; loops: Animation.Infinite; running: lib.visible }
    }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.5 }

    // ---------------------------------------------------------------- header
    Item {
        id: header
        width: parent.width
        height: Theme.px(84)
        Row {
            x: Theme.px(32)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(14)
            NeonText { text: "ZEN1"; size: Theme.px(40); color: "#eafcff"; glowColor: Theme.cyan }
            NeonText {
                anchors.bottom: parent.bottom
                text: "ARCADE"; size: Theme.px(16); color: "#ffe6fa"; glowColor: Theme.pink; spacing: Theme.px(4)
            }
        }
        Row {
            anchors.right: parent.right; anchors.rightMargin: Theme.px(32)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(22)
            Repeater {
                model: [["1P", zen1.player1Assigned], ["2P", zen1.player2Assigned]]
                PixelText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData[0]
                    size: 16
                    color: modelData[1] ? Theme.green : "#3a3850"
                }
            }
            PixelText {
                id: clock
                anchors.verticalCenter: parent.verticalCenter
                size: 24
                color: Theme.amber
                text: Qt.formatTime(new Date(), "hh:mm")
                Timer { interval: 5000; running: true; repeat: true; onTriggered: clock.text = Qt.formatTime(new Date(), "hh:mm") }
            }
            Rectangle {  // SETUP: reached by pushing up from the top row
                anchors.verticalCenter: parent.verticalCenter
                width: setupText.implicitWidth + Theme.px(28); height: Theme.px(40)
                color: lib.zone === "setup" ? Qt.rgba(0.16, 0.9, 1, 0.22) : "transparent"
                border.color: lib.zone === "setup" ? Theme.cyan : "#3a3850"
                border.width: Math.max(2, Theme.px(2))
                PixelText { id: setupText; anchors.centerIn: parent; text: "SETUP"; size: 16; color: lib.zone === "setup" ? Theme.text : Theme.dim }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Math.max(2, Theme.px(2)); color: Theme.pink; opacity: 0.6 }
    }

    // ---------------------------------------------------------------- grid
    GridView {
        id: grid
        x: Theme.px(36)
        y: header.height + Theme.px(22)
        width: parent.width - 2 * x
        height: Theme.px(622)
        cellWidth: width / lib.columns
        cellHeight: Theme.px(311)
        clip: true
        model: lib.games
        interactive: false
        keyNavigationWraps: false
        highlightMoveDuration: 140
        preferredHighlightBegin: Theme.px(8)
        preferredHighlightEnd: height - Theme.px(8)
        highlightRangeMode: GridView.ApplyRange
        cacheBuffer: Theme.px(320)
        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight
            GameCard {
                anchors.centerIn: parent
                width: parent.width - Theme.px(30)
                height: parent.height - Theme.px(30)
                game: modelData
                selected: index === grid.currentIndex && lib.zone === "grid"
                pulse: lib.pulse
            }
        }
    }
    // Scroll position: a pixel bar on the right
    Rectangle {
        visible: grid.contentHeight > grid.height
        x: parent.width - Theme.px(18); y: grid.y
        width: Theme.px(6); height: grid.height
        color: "#1d1b2e"
        Rectangle {
            width: parent.width
            height: Math.max(Theme.px(24), parent.height * grid.height / Math.max(1, grid.contentHeight))
            y: (parent.height - height) * (grid.contentY / Math.max(1, grid.contentHeight - grid.height))
            color: Theme.cyan
        }
    }

    // Empty Games folder
    Column {
        anchors.horizontalCenter: grid.horizontalCenter
        y: grid.y + Theme.px(170)
        spacing: Theme.px(22)
        visible: lib.games.length === 0
        NeonText { anchors.horizontalCenter: parent.horizontalCenter; text: "NO GAMES YET"; size: Theme.px(40); color: Theme.text; glowColor: Theme.violet }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            font.family: Theme.mono
            font.pixelSize: Theme.px(28)
            color: Theme.dim
            text: "Put ROM sets (.zip, current MAME versions) in\n" + zen1.gamesDir
        }
    }

    InfoPanel {
        x: Theme.px(28)
        width: parent.width - 2 * x
        y: grid.y + grid.height + Theme.px(14)
        height: parent.height - y - Theme.px(18)
        game: lib.current
    }
}
