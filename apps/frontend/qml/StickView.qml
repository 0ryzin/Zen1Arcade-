// SPDX-License-Identifier: GPL-3.0-or-later
// A joystick seen from above: the stick position (analogue sticks), the
// dead zone, and eight direction lamps.
import QtQuick
import Zen1 1.0

Item {
    id: sv
    property var live: ({})
    property real deadzone: live.deadzone !== undefined ? live.deadzone : 0.5
    property var reached: []      // directions already reached (test step), "dx,dy" strings
    readonly property real r: width / 2

    Rectangle { anchors.fill: parent; radius: width / 2; color: "#0b0a1a"; border.color: "#3a3850"; border.width: Math.max(2, Theme.px(3)) }
    Rectangle {  // dead zone
        visible: !!sv.live.hasAxes && !sv.live.digital
        anchors.centerIn: parent
        width: parent.width * sv.deadzone; height: width; radius: width / 2
        color: Qt.rgba(1, 0.24, 0.78, 0.12); border.color: Theme.pink; border.width: Math.max(1, Theme.px(2))
    }
    Repeater {  // direction lamps
        model: [[0, -1], [1, -1], [1, 0], [1, 1], [0, 1], [-1, 1], [-1, 0], [-1, -1]]
        Rectangle {
            readonly property bool on: sv.live.dirX === modelData[0] && sv.live.dirY === modelData[1]
            readonly property bool done: sv.reached.indexOf(modelData[0] + "," + modelData[1]) >= 0
            width: Theme.px(22); height: width
            x: sv.r + modelData[0] * sv.r * 0.82 / (modelData[0] && modelData[1] ? 1.414 : 1) - width / 2
            y: sv.r + modelData[1] * sv.r * 0.82 / (modelData[0] && modelData[1] ? 1.414 : 1) - height / 2
            color: on ? Theme.amber : done ? Theme.green : "#2a2838"
        }
    }
    Rectangle {  // the stick
        readonly property real px: sv.live.hasAxes ? sv.live.x : (sv.live.dirX || 0) * 0.8
        readonly property real py: sv.live.hasAxes ? sv.live.y : (sv.live.dirY || 0) * 0.8
        width: sv.width * 0.2; height: width; radius: width / 2
        x: sv.r + px * sv.r * 0.75 - width / 2
        y: sv.r + py * sv.r * 0.75 - height / 2
        color: sv.live.connected ? Theme.cyan : "#3a3850"
        border.color: "white"; border.width: Math.max(1, Theme.px(2))
    }
}
