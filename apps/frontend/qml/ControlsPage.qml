// SPDX-License-Identifier: GPL-3.0-or-later
// Controllers: which panel is Player 1 / Player 2, assign by pressing a
// button, remap the six buttons and the player button, swap, reset, test.
import QtQuick
import Zen1 1.0

Item {
    id: page
    signal back()
    property int sel: 0
    readonly property var rows: [
        { label: "Assign the Player 1 panel", action: function() { zen1.startAssign(0) } },
        { label: "Assign the Player 2 panel", action: function() { zen1.startAssign(1) } },
        { label: "Set Player 1 buttons", action: function() { zen1.startRemap(0) } },
        { label: "Set Player 2 buttons", action: function() { zen1.startRemap(1) } },
        { label: "Swap Player 1 and Player 2", action: function() { zen1.swapPlayers() } },
        { label: "Reset controls to defaults", action: function() { zen1.resetControls() } },
        { label: "Detect panels again", action: function() { zen1.refreshControllers() } },
        { label: "Cabinet test card", action: function() { zen1.launchTestCard() } },
        { label: "Back", action: function() { page.back() } }
    ]
    function handle(name) {
        if (zen1.captureActive) return
        if (name === "up") sel = (sel + rows.length - 1) % rows.length
        else if (name === "down") sel = (sel + 1) % rows.length
        else if (name === "lp" || name === "player") rows[sel].action()
        else if (name === "lk") back()
    }

    Backdrop { anchors.fill: parent; horizon: 0.8; lineOpacity: 0.35 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.6 }
    NeonText { x: Theme.px(60); y: Theme.px(44); text: "CONTROLLERS"; size: Theme.px(56); color: "#eafcff"; glowColor: Theme.cyan; spacing: Theme.px(6) }

    // Player status cards
    Row {
        x: Theme.px(60)
        y: Theme.px(140)
        spacing: Theme.px(24)
        Repeater {
            model: [[1, zen1.player1Assigned, zen1.player1], [2, zen1.player2Assigned, zen1.player2]]
            Rectangle {
                width: (page.width - Theme.px(144)) / 2
                height: Theme.px(150)
                radius: Theme.px(14)
                color: Qt.rgba(0, 0, 0, 0.45)
                border.color: modelData[1] ? Theme.green : Theme.amber
                border.width: Math.max(2, Theme.px(2))
                Column {
                    x: Theme.px(20); y: Theme.px(14)
                    width: parent.width - Theme.px(40)
                    spacing: Theme.px(6)
                    NeonText { text: "PLAYER " + modelData[0]; size: Theme.px(32); color: Theme.text; glowColor: modelData[1] ? Theme.green : Theme.amber }
                    Text { width: parent.width; elide: Text.ElideRight; text: modelData[2]; color: modelData[1] ? Theme.text : Theme.amber; font.pixelSize: Theme.px(19); font.bold: true }
                    // Panel layout: LP MP HP / LK MK HK + player button
                    Row {
                        spacing: Theme.px(8)
                        Repeater {
                            model: ["LP", "MP", "HP", "LK", "MK", "HK", "P" + modelData[0]]
                            Rectangle {
                                width: Theme.px(34); height: width; radius: width / 2
                                color: "transparent"
                                border.color: index < 3 ? Theme.cyan : index < 6 ? Theme.pink : Theme.amber
                                border.width: Math.max(2, Theme.px(2))
                                Text { anchors.centerIn: parent; text: modelData; color: Theme.text; font.pixelSize: Theme.px(11); font.bold: true }
                            }
                        }
                    }
                }
            }
        }
    }

    Column {
        x: Theme.px(60)
        y: Theme.px(320)
        width: parent.width - 2 * x
        spacing: Theme.px(8)
        Repeater {
            model: page.rows
            Rectangle {
                readonly property bool on: index === page.sel
                width: parent.width
                height: Theme.px(52)
                radius: Theme.px(10)
                color: on ? Qt.rgba(0.16, 0.9, 1, 0.16) : Qt.rgba(1, 1, 1, 0.04)
                border.color: on ? Theme.cyan : "transparent"
                border.width: Math.max(1, Theme.px(2))
                Text { x: Theme.px(22); anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.text; font.pixelSize: Theme.px(23); font.bold: true }
            }
        }
    }
    Text {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: Theme.px(40)
        wrapMode: Text.WordWrap
        text: "Panels are recognised by USB port, serial or name, never by plug-in order. The Steam Deck's own controls are never made Player 1 or 2; in a game they only stand in while no Player 1 panel is connected."
        color: Theme.dim; font.pixelSize: Theme.px(18)
    }

    // Capture overlay: "press a button"
    Rectangle {
        anchors.fill: parent
        visible: zen1.captureActive
        color: Qt.rgba(0.01, 0, 0.05, 0.9)
        Timer { interval: 30000; running: zen1.captureActive; onTriggered: zen1.cancelCapture() }
        Column {
            anchors.centerIn: parent
            width: parent.width * 0.85
            spacing: Theme.px(26)
            NeonText { anchors.horizontalCenter: parent.horizontalCenter; text: "PRESS A BUTTON"; size: Theme.px(60); color: Theme.text; glowColor: Theme.pink; spacing: Theme.px(6) }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: zen1.capturePrompt
                color: Theme.cyan; font.pixelSize: Theme.px(32); font.bold: true
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: "Cancels by itself after 30 seconds (or Esc on a keyboard)"
                color: Theme.dim; font.pixelSize: Theme.px(20)
            }
        }
    }
}
