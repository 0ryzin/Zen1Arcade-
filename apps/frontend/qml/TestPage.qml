// SPDX-License-Identifier: GPL-3.0-or-later
// Live controller test: both panels, every button, the stick and how 8-way,
// 4-way and 2-way games will read it. Panel buttons are being tested here,
// so they do nothing else: hold the 1P button for 2 seconds to leave.
import QtQuick
import Zen1 1.0

Item {
    id: page
    signal back()
    property real hold: 0

    function handle(name) { }  // every panel input is shown, not obeyed
    function dirName(dx, dy) {
        const n = { "0,-1": "UP", "1,-1": "UP-RIGHT", "1,0": "RIGHT", "1,1": "DOWN-RIGHT", "0,1": "DOWN",
                    "-1,1": "DOWN-LEFT", "-1,0": "LEFT", "-1,-1": "UP-LEFT" }
        return n[dx + "," + dy] || "CENTRE"
    }

    Timer {
        interval: 50; repeat: true; running: page.visible
        onTriggered: {
            const l = zen1.p1Live
            const pressed = l.connected && l.buttons && l.buttons[l.map[6]]
            page.hold = pressed ? page.hold + 0.05 : 0
            if (page.hold >= 2.0) { page.hold = 0; zen1.sfx("back"); page.back() }
        }
    }

    Backdrop { anchors.fill: parent; horizon: 0.9; lineOpacity: 0.25 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.75 }
    NeonText { x: Theme.px(56); y: Theme.px(36); text: "CONTROLLER TEST"; size: Theme.px(40); color: "#eafcff"; glowColor: Theme.cyan }

    Row {
        x: Theme.px(40); y: Theme.px(120)
        spacing: Theme.px(40)
        Repeater {
            model: 2
            Rectangle {
                id: half
                readonly property var live: index === 0 ? zen1.p1Live : zen1.p2Live
                property int p4x: 0
                property int p4y: 0
                width: (page.width - Theme.px(120)) / 2
                height: page.height - Theme.px(230)
                color: Qt.rgba(0, 0, 0, 0.45)
                border.color: live.connected ? (index === 0 ? Theme.cyan : Theme.pink) : "#3a3850"
                border.width: Math.max(2, Theme.px(2))

                PixelText { x: Theme.px(18); y: Theme.px(18); text: "PLAYER " + (index + 1); size: 24; color: half.border.color }
                Text {
                    x: Theme.px(18); y: Theme.px(52); width: parent.width - Theme.px(36); elide: Text.ElideRight
                    text: half.live.connected ? half.live.name : "No panel assigned"
                    font.family: Theme.mono; font.pixelSize: Theme.px(24); color: half.live.connected ? Theme.text : Theme.amber
                }
                StickView {
                    x: Theme.px(18); y: Theme.px(96)
                    width: Theme.px(220); height: width
                    live: half.live
                }
                Column {
                    x: Theme.px(256); y: Theme.px(104)
                    spacing: Theme.px(6)
                    readonly property int dx: half.live.dirX || 0
                    readonly property int dy: half.live.dirY || 0
                    PixelText { text: "8-WAY " + page.dirName(parent.dx, parent.dy); size: 8; color: Theme.cyan }
                    PixelText { text: "4-WAY " + page.dirName(parent.dx !== 0 && parent.dy !== 0 ? 0 : parent.dx, parent.dy); size: 8; color: Theme.cyan }
                    PixelText { text: "2-WAY " + page.dirName(parent.dx, 0); size: 8; color: Theme.cyan }
                    Text {
                        visible: !!half.live.hasAxes
                        text: "x " + (half.live.rawX || 0).toFixed(2) + "  y " + (half.live.rawY || 0).toFixed(2)
                              + (half.live.calibrated ? (half.live.digital ? "  digital" : "  calibrated") : "  default")
                        font.family: Theme.mono; font.pixelSize: Theme.px(22); color: Theme.dim
                    }
                }
                // Arcade layout from the button map
                Grid {
                    x: Theme.px(18); y: Theme.px(340)
                    columns: 4
                    spacing: Theme.px(14)
                    Repeater {
                        model: 7
                        ArcadeButton {
                            readonly property int idx: half.live.map ? half.live.map[index] : index
                            diameter: Theme.px(56)
                            label: ["LP", "MP", "HP", "1P", "LK", "MK", "HK"][index]
                            color: index === 3 ? Theme.amber : index < 3 ? Theme.cyan : Theme.pink
                            readonly property int slot: [0, 1, 2, 6, 3, 4, 5][index]
                            lit: !!half.live.buttons && !!half.live.buttons[half.live.map ? half.live.map[slot] : slot]
                        }
                    }
                }
                // Raw button numbers
                Grid {
                    x: Theme.px(18); anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.px(18)
                    columns: 6
                    spacing: Theme.px(6)
                    Repeater {
                        model: 12
                        Rectangle {
                            width: Theme.px(44); height: Theme.px(34)
                            color: half.live.buttons && half.live.buttons[index] ? Theme.amber : "#1d1b2e"
                            PixelText { anchors.centerIn: parent; text: index + 1; size: 8; color: half.live.buttons && half.live.buttons[index] ? "black" : Theme.dim }
                        }
                    }
                }
            }
        }
    }

    // Leave: hold 1P
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.px(28)
        spacing: Theme.px(10)
        PixelText { anchors.horizontalCenter: parent.horizontalCenter; text: "HOLD 1P FOR 2 SECONDS TO LEAVE"; size: 16; color: Theme.dim }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.px(480); height: Theme.px(12); color: "#1d1b2e"
            Rectangle { width: parent.width * Math.min(1, page.hold / 2); height: parent.height; color: Theme.pink }
        }
    }
}
