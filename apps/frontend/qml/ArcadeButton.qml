// SPDX-License-Identifier: GPL-3.0-or-later
// One round arcade button seen from above, with its label inside.
import QtQuick
import Zen1 1.0

Item {
    id: b
    property color color: Theme.cyan
    property string label: ""
    property bool lit: false
    property bool used: true
    property real diameter: Theme.px(48)
    width: diameter; height: diameter

    Rectangle {  // bezel ring
        anchors.fill: parent
        radius: width / 2
        color: "#15131f"
        border.color: b.used ? Qt.darker(b.color, 1.6) : "#2a2838"
        border.width: Math.max(2, b.diameter * 0.08)
    }
    Rectangle {  // the plunger
        anchors.centerIn: parent
        width: parent.width * 0.74; height: width; radius: width / 2
        color: !b.used ? "#1d1b29" : b.lit ? Qt.lighter(b.color, 1.3) : Qt.darker(b.color, 1.35)
        Rectangle {  // highlight
            x: parent.width * 0.2; y: parent.height * 0.14
            width: parent.width * 0.36; height: parent.height * 0.2; radius: height / 2
            color: "white"; opacity: b.used ? 0.28 : 0.05
        }
    }
    Repeater {  // glow when lit
        model: b.lit ? 3 : 0
        Rectangle {
            anchors.centerIn: parent
            width: b.diameter + Theme.px(6 + index * 6); height: width; radius: width / 2
            color: "transparent"; border.color: b.color; border.width: Math.max(1, Theme.px(2))
            opacity: 0.5 - index * 0.15
        }
    }
    PixelText {
        anchors.centerIn: parent
        text: b.label
        size: 8
        color: b.used ? "white" : "#55526a"
    }
}
