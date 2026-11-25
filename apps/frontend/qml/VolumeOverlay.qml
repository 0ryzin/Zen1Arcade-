// SPDX-License-Identifier: GPL-3.0-or-later
// "VOLUME 50%" panel shown for a moment whenever the cabinet volume changes:
// a pixel speaker (crossed out when muted) and three level bars, sized to be
// read from across the room.
import QtQuick
import Zen1 1.0

Item {
    id: ov
    anchors.fill: parent
    visible: opacity > 0
    opacity: 0
    Behavior on opacity { NumberAnimation { duration: 160 } }

    function show() { opacity = 1; hideTimer.restart() }
    Timer { id: hideTimer; interval: 1800; onTriggered: ov.opacity = 0 }
    Connections { target: zen1; function onVolumeShown() { ov.show() } }

    readonly property int level: zen1.volume >= 100 ? 2 : zen1.volume >= 50 ? 1 : 0

    Rectangle {
        anchors.centerIn: parent
        width: Theme.px(640); height: Theme.px(300)
        color: Qt.rgba(0.03, 0.01, 0.09, 0.95)
        border.color: Theme.pink
        border.width: Math.max(4, Theme.px(5))

        // Speaker: body + cone
        Item {
            id: speaker
            x: Theme.px(56); y: Theme.px(46)
            width: Theme.px(130); height: Theme.px(130)
            Rectangle { x: 0; y: parent.height * 0.32; width: parent.width * 0.28; height: parent.height * 0.36; color: Theme.cyan }
            Repeater {
                model: 8
                Rectangle {
                    x: speaker.width * 0.28 + index * speaker.width * 0.05
                    y: speaker.height * (0.32 - index * 0.035)
                    width: speaker.width * 0.05 + 1
                    height: speaker.height * (0.36 + index * 0.07)
                    color: Theme.cyan
                }
            }
            // Muted: red X
            Repeater {
                model: ov.level === 0 ? 2 : 0
                Rectangle {
                    x: speaker.width * 0.74; y: speaker.height * 0.46
                    width: speaker.width * 0.34; height: Math.max(6, Theme.px(10))
                    color: Theme.red
                    rotation: index === 0 ? 45 : -45
                }
            }
            // Sound waves for 50 % and 100 %
            Repeater {
                model: ov.level
                Rectangle {
                    x: speaker.width * (0.8 + index * 0.16)
                    y: speaker.height * (0.32 - index * 0.12)
                    width: Math.max(5, Theme.px(8))
                    height: speaker.height * (0.36 + index * 0.24)
                    color: Theme.cyan
                    opacity: 0.85
                }
            }
        }

        // Three level bars: MUTED / LOW / FULL
        Row {
            x: Theme.px(260); y: Theme.px(56)
            spacing: Theme.px(26)
            Repeater {
                model: 3
                Item {
                    width: Theme.px(82); height: Theme.px(120)
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * (0.4 + index * 0.3)
                        color: index === 0 ? (ov.level === 0 ? Theme.red : Theme.cyan)
                              : index <= ov.level ? (index === 2 ? Theme.pink : Theme.cyan) : "#2e2a44"
                        border.color: index === ov.level ? Theme.text : "transparent"
                        border.width: Math.max(2, Theme.px(3))
                    }
                }
            }
        }
        Row {
            x: Theme.px(260); y: Theme.px(186)
            spacing: Theme.px(26)
            Repeater {
                model: ["0", "50", "100"]
                PixelText { width: Theme.px(82); horizontalAlignment: Text.AlignHCenter; text: modelData; size: 8; color: index === ov.level ? Theme.text : Theme.dim }
            }
        }
        NeonText {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Theme.px(222)
            text: "VOLUME " + zen1.volume + "%"
            size: Theme.px(44)
            color: Theme.text
            glowColor: ov.level === 0 ? Theme.red : Theme.cyan
        }
    }
}
