// SPDX-License-Identifier: GPL-3.0-or-later
// Controllers: which panel is Player 1 / Player 2 (assign by pressing a
// button on it), swap, unassign, button layout, joystick calibration, test.
import QtQuick
import Zen1 1.0

Item {
    id: page
    signal back()
    signal openTest()
    signal openCalibrate(int player)
    property alias sel: menu.sel

    function handle(name) {
        if (zen1.captureActive) return
        if (menu.handle(name)) return
        if (name === "lk") { zen1.sfx("back"); back() }
    }

    Backdrop { anchors.fill: parent; horizon: 0.86; lineOpacity: 0.3 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.7 }
    NeonText { x: Theme.px(56); y: Theme.px(36); text: "CONTROLLERS"; size: Theme.px(48); color: "#eafcff"; glowColor: Theme.cyan }

    MenuList {
        id: menu
        x: Theme.px(56); y: Theme.px(116)
        width: Theme.px(600)
        height: parent.height - y - Theme.px(40)
        rows: [
            { header: "PLAYERS" },
            { label: "Assign Player 1 panel", value: "PRESS", kind: "action", run: function() { zen1.startAssign(0) } },
            { label: "Assign Player 2 panel", value: "PRESS", kind: "action", run: function() { zen1.startAssign(1) } },
            { label: "Swap Player 1 and 2", value: ">", kind: "action", run: function() { zen1.swapPlayers() } },
            { label: "Unassign Player 1", value: ">", kind: "action", run: function() { zen1.unassign(0) } },
            { label: "Unassign Player 2", value: ">", kind: "action", run: function() { zen1.unassign(1) } },
            { header: "BUTTONS" },
            { label: "Set Player 1 buttons", value: ">", kind: "action", run: function() { zen1.startRemap(0) } },
            { label: "Set Player 2 buttons", value: ">", kind: "action", run: function() { zen1.startRemap(1) } },
            { header: "JOYSTICKS" },
            { label: "Calibrate Player 1 stick", value: ">", kind: "action", run: function() { page.openCalibrate(0) } },
            { label: "Calibrate Player 2 stick", value: ">", kind: "action", run: function() { page.openCalibrate(1) } },
            { label: "Reset Player 1 calibration", value: ">", kind: "action", run: function() { zen1.calibrationReset(0) } },
            { label: "Reset Player 2 calibration", value: ">", kind: "action", run: function() { zen1.calibrationReset(1) } },
            { header: "TEST" },
            { label: "Controller test", value: ">", kind: "action", run: function() { page.openTest() } },
            { label: "Cabinet test card", value: ">", kind: "action", run: function() { zen1.launchTestCard() } },
            { label: "Detect panels again", value: ">", kind: "action", run: function() { zen1.refreshControllers() } },
            { label: "Reset all controls", value: ">", kind: "action", danger: true, run: function() { zen1.resetControls() } }
        ]
    }

    // Right side: players and every detected controller
    Column {
        x: Theme.px(690); y: Theme.px(124)
        width: parent.width - x - Theme.px(56)
        spacing: Theme.px(14)
        Repeater {
            model: [[1, zen1.player1Assigned, zen1.player1], [2, zen1.player2Assigned, zen1.player2]]
            Rectangle {
                width: parent.width; height: Theme.px(96)
                color: Qt.rgba(0, 0, 0, 0.5)
                border.color: modelData[1] ? Theme.green : Theme.amber
                border.width: Math.max(2, Theme.px(2))
                PixelText { x: Theme.px(16); y: Theme.px(14); text: "PLAYER " + modelData[0]; size: 16; color: modelData[1] ? Theme.green : Theme.amber }
                Text {
                    x: Theme.px(16); y: Theme.px(44); width: parent.width - Theme.px(32)
                    elide: Text.ElideRight
                    text: modelData[1] ? modelData[2] : "No panel assigned"
                    font.family: Theme.mono; font.pixelSize: Theme.px(26); color: Theme.text
                }
            }
        }
        PixelText { text: "DETECTED CONTROLLERS"; size: 8; color: Theme.dim; topPadding: Theme.px(8) }
        Repeater {
            model: zen1.devices
            Rectangle {
                width: parent.width; height: Theme.px(64)
                color: Qt.rgba(1, 1, 1, 0.04)
                border.color: modelData.player > 0 ? Theme.cyan : "#2a2838"
                border.width: Math.max(1, Theme.px(2))
                Text {
                    x: Theme.px(12); y: Theme.px(6); width: parent.width - Theme.px(24)
                    elide: Text.ElideRight
                    text: modelData.name
                    font.family: Theme.mono; font.pixelSize: Theme.px(24); color: Theme.text
                }
                Text {
                    x: Theme.px(12); y: Theme.px(34); width: parent.width - Theme.px(24)
                    elide: Text.ElideRight
                    text: modelData.ids + (modelData.serial ? "  serial " + modelData.serial : "") + (modelData.port ? "  " + modelData.port : "") + "  -  " + modelData.note
                    font.family: Theme.mono; font.pixelSize: Theme.px(20)
                    color: modelData.player > 0 ? Theme.cyan : Theme.dim
                }
            }
        }
        Text {
            width: parent.width
            visible: zen1.devices.length === 0
            wrapMode: Text.WordWrap
            text: "No arcade panels connected right now. Plug them in, then choose Detect panels again."
            font.family: Theme.mono; font.pixelSize: Theme.px(24); color: Theme.dim
        }
    }

    // "Press a button" overlay for assignment and button setup
    Rectangle {
        anchors.fill: parent
        visible: zen1.captureActive
        color: Qt.rgba(0.01, 0, 0.05, 0.92)
        Timer { interval: 30000; running: zen1.captureActive; onTriggered: zen1.cancelCapture() }
        Column {
            anchors.centerIn: parent
            width: parent.width * 0.85
            spacing: Theme.px(26)
            NeonText { anchors.horizontalCenter: parent.horizontalCenter; text: "PRESS A BUTTON"; size: Theme.px(48); color: Theme.text; glowColor: Theme.pink }
            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                text: zen1.capturePrompt
                font.family: Theme.mono; font.pixelSize: Theme.px(40); color: Theme.cyan
            }
            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter
                text: "Cancels by itself after 30 seconds"
                font.family: Theme.mono; font.pixelSize: Theme.px(26); color: Theme.dim
            }
        }
    }
}
