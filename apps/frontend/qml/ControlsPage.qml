// SPDX-License-Identifier: GPL-3.0-or-later
// Controllers: every detected controller with its name and hardware IDs;
// pick one and assign it to Player 1, Player 2 or both (one board wired to
// both panels). Also: assign by pressing a button, swap, unassign, detect
// again, each player's buttons, joystick calibration and the live test.
// The Steam Deck's own controls are listed but can never be a player.
import QtQuick
import Zen1 1.0

Item {
    id: page
    signal back()
    signal openTest()
    signal openCalibrate(int player)
    signal openButtons(int player)
    property alias sel: menu.sel

    // Focus: "menu" (left) or "devices" (the list on the right)
    property string focusArea: "menu"
    property int devSel: 0
    property bool choosing: false
    property int choice: 0
    readonly property var choices: ["PLAYER 1", "PLAYER 2", "BOTH PLAYERS", "CANCEL"]
    property var devs: zen1.devices
    Connections { target: zen1; function onChanged() { page.devs = zen1.devices } }
    onVisibleChanged: if (visible) { devs = zen1.devices; focusArea = "menu"; choosing = false }

    function handle(name) {
        if (zen1.captureActive) return
        if (focusArea === "devices") {
            if (choosing) {
                if (name === "left") { choice = (choice + 3) % 4; zen1.sfx("move") }
                else if (name === "right") { choice = (choice + 1) % 4; zen1.sfx("move") }
                else if (name === "lp" || name === "player") {
                    choosing = false
                    if (choice < 3) zen1.assignDevice(devs[devSel].index, choice)
                    else zen1.sfx("back")
                } else if (name === "lk") { choosing = false; zen1.sfx("back") }
                return
            }
            if (name === "up") { devSel = Math.max(0, devSel - 1); zen1.sfx("move") }
            else if (name === "down") { devSel = Math.min(devs.length - 1, devSel + 1); zen1.sfx("move") }
            else if ((name === "lp" || name === "player") && devs.length > 0) {
                if (!devs[devSel].candidate) { zen1.sfx("error"); zen1.assignDevice(devs[devSel].index, 0); return }
                choice = 0; choosing = true; zen1.sfx("select")
            } else if (name === "lk" || name === "left") { focusArea = "menu"; zen1.sfx("back") }
            return
        }
        if (menu.handle(name)) return
        if (name === "lk") { zen1.sfx("back"); back() }
    }

    Backdrop { anchors.fill: parent; horizon: 0.86; lineOpacity: 0.3 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.7 }
    NeonText { x: Theme.px(56); y: Theme.px(36); text: "CONTROLLERS"; size: Theme.px(48); color: "#eafcff"; glowColor: Theme.cyan }

    MenuList {
        id: menu
        x: Theme.px(56); y: Theme.px(116)
        width: Theme.px(560)
        height: parent.height - y - Theme.px(40)
        opacity: page.focusArea === "menu" ? 1 : 0.55
        rows: [
            { header: "PLAYERS" },
            { label: "Choose from detected list", value: ">", kind: "action", run: function() {
                page.devs = zen1.devices; page.devSel = 0; page.focusArea = "devices" } },
            { label: "Assign P1 by pressing", value: "PRESS", kind: "action", run: function() { zen1.startAssign(0) } },
            { label: "Assign P2 by pressing", value: "PRESS", kind: "action", run: function() { zen1.startAssign(1) } },
            { label: "Swap Player 1 and 2", value: ">", kind: "action", run: function() { zen1.swapPlayers() } },
            { label: "Unassign Player 1", value: ">", kind: "action", run: function() { zen1.unassign(0) } },
            { label: "Unassign Player 2", value: ">", kind: "action", run: function() { zen1.unassign(1) } },
            { label: "Detect controllers again", value: ">", kind: "action", run: function() { zen1.refreshControllers(); page.devs = zen1.devices } },
            { header: "BUTTONS" },
            { label: "Player 1 buttons", value: ">", kind: "action", run: function() { page.openButtons(0) } },
            { label: "Player 2 buttons", value: ">", kind: "action", run: function() { page.openButtons(1) } },
            { header: "JOYSTICKS" },
            { label: "Calibrate Player 1 stick", value: ">", kind: "action", run: function() { page.openCalibrate(0) } },
            { label: "Calibrate Player 2 stick", value: ">", kind: "action", run: function() { page.openCalibrate(1) } },
            { label: "Reset P1 calibration", value: ">", kind: "action", run: function() { zen1.calibrationReset(0) } },
            { label: "Reset P2 calibration", value: ">", kind: "action", run: function() { zen1.calibrationReset(1) } },
            { header: "TEST" },
            { label: "Controller test", value: ">", kind: "action", run: function() { page.openTest() } },
            { label: "Cabinet test card", value: ">", kind: "action", run: function() { zen1.launchTestCard() } },
            { label: "Reset all controls", value: ">", kind: "action", danger: true, run: function() { zen1.resetControls() } }
        ]
    }

    // Right side: players and every detected controller
    Column {
        x: Theme.px(650); y: Theme.px(124)
        width: parent.width - x - Theme.px(48)
        spacing: Theme.px(12)
        Repeater {
            model: [[1, zen1.player1Assigned, zen1.player1], [2, zen1.player2Assigned, zen1.player2]]
            Rectangle {
                width: parent.width; height: Theme.px(88)
                color: Qt.rgba(0, 0, 0, 0.5)
                border.color: modelData[1] ? Theme.green : Theme.amber
                border.width: Math.max(2, Theme.px(2))
                PixelText { x: Theme.px(16); y: Theme.px(12); text: "PLAYER " + modelData[0]; size: 16; color: modelData[1] ? Theme.green : Theme.amber }
                Text {
                    x: Theme.px(16); y: Theme.px(42); width: parent.width - Theme.px(32)
                    elide: Text.ElideRight
                    text: modelData[1] ? modelData[2] : "No controller assigned"
                    font.family: Theme.mono; font.pixelSize: Theme.px(26); color: Theme.text
                }
            }
        }
        PixelText {
            text: page.focusArea === "devices" ? "DETECTED CONTROLLERS - LP CHOOSE  LK BACK" : "DETECTED CONTROLLERS"
            size: 8; color: page.focusArea === "devices" ? Theme.cyan : Theme.dim; topPadding: Theme.px(8)
        }
        Repeater {
            model: page.devs
            Rectangle {
                readonly property bool on: page.focusArea === "devices" && index === page.devSel
                width: parent.width; height: on && page.choosing ? Theme.px(150) : Theme.px(96)
                color: on ? Qt.rgba(0.16, 0.9, 1, 0.14) : Qt.rgba(1, 1, 1, 0.04)
                border.color: on ? Theme.cyan : modelData.player > 0 ? Theme.green : "#2a2838"
                border.width: Math.max(2, Theme.px(on ? 3 : 2))
                Text {
                    x: Theme.px(12); y: Theme.px(6); width: parent.width - Theme.px(24)
                    elide: Text.ElideRight
                    text: modelData.name
                    font.family: Theme.mono; font.pixelSize: Theme.px(26); color: modelData.candidate ? Theme.text : Theme.dim
                }
                Text {
                    x: Theme.px(12); y: Theme.px(36); width: parent.width - Theme.px(24)
                    elide: Text.ElideRight
                    text: "USB " + modelData.ids + (modelData.serial ? "  serial " + modelData.serial : "  no serial") + "  " + modelData.detail
                    font.family: Theme.mono; font.pixelSize: Theme.px(20); color: Theme.dim
                }
                Text {
                    x: Theme.px(12); y: Theme.px(62); width: parent.width - Theme.px(24)
                    elide: Text.ElideMiddle
                    text: modelData.note + (modelData.port ? "   port " + modelData.port : "   " + modelData.node)
                    font.family: Theme.mono; font.pixelSize: Theme.px(20)
                    color: modelData.player > 0 ? Theme.green : modelData.candidate ? Theme.dim : Theme.amber
                }
                Row {
                    visible: parent.on && page.choosing
                    x: Theme.px(12); y: Theme.px(100)
                    spacing: Theme.px(10)
                    Repeater {
                        model: page.choices
                        Rectangle {
                            width: Theme.px(150); height: Theme.px(38)
                            color: index === page.choice ? Theme.cyan : Qt.rgba(0, 0, 0, 0.5)
                            border.color: Theme.cyan; border.width: Math.max(1, Theme.px(2))
                            PixelText { anchors.centerIn: parent; text: modelData; size: 8; color: index === page.choice ? Theme.bg : Theme.text }
                        }
                    }
                }
            }
        }
        Text {
            width: parent.width
            visible: page.devs.length === 0
            wrapMode: Text.WordWrap
            text: "No controllers connected right now. Plug them in, then choose Detect controllers again. The Steam Deck's own controls still work the menus."
            font.family: Theme.mono; font.pixelSize: Theme.px(24); color: Theme.dim
        }
    }

    CaptureOverlay { anchors.fill: parent }
}
