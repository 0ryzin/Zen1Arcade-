// SPDX-License-Identifier: GPL-3.0-or-later
// Zen1 neon backdrop: violet sky, glowing horizon, perspective grid floor.
// `phase` (0..1, wraps) moves the floor toward the viewer.
import QtQuick
import Zen1 1.0

Item {
    id: bd
    property real phase: 0
    property real horizon: 0.58       // fraction of the height
    property real lineOpacity: 1.0
    property color gridColor: Theme.pink
    clip: true

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#03020a" }
            GradientStop { position: bd.horizon * 0.85; color: "#160a36" }
            GradientStop { position: bd.horizon; color: "#3b0f5c" }
            GradientStop { position: Math.min(1, bd.horizon + 0.02); color: "#12062a" }
            GradientStop { position: 1.0; color: "#05030f" }
        }
    }
    // Horizon glow
    Repeater {
        model: 4
        Rectangle {
            width: bd.width
            height: Math.max(1, bd.height * (0.004 + index * 0.012))
            y: bd.height * bd.horizon - height / 2
            color: index === 0 ? "#ffd6f6" : Theme.pink
            opacity: [0.9, 0.35, 0.18, 0.08][index] * bd.lineOpacity
        }
    }
    Item {
        id: floor
        y: bd.height * bd.horizon
        width: bd.width
        height: bd.height - y
        clip: true
        // Rows: denser toward the horizon, sliding toward the viewer.
        Repeater {
            model: 16
            Rectangle {
                readonly property real d: (index + bd.phase) / 16
                width: floor.width
                height: Math.max(1, 1 + d * 3 * Theme.u)
                y: floor.height * Math.pow(d, 2.1)
                color: bd.gridColor
                opacity: Math.min(1, d * 2.5) * 0.85 * bd.lineOpacity
            }
        }
        // Columns converging on the vanishing point.
        Repeater {
            model: 23
            Rectangle {
                readonly property real dx: (index - 11) * floor.width * 0.17
                readonly property real len: Math.sqrt(dx * dx + floor.height * floor.height)
                x: floor.width / 2 - width / 2
                y: 0
                width: Math.max(1, 2 * Theme.u)
                height: len
                transformOrigin: Item.Top
                rotation: -Math.atan2(dx, floor.height) * 180 / Math.PI
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: bd.gridColor }
                }
                opacity: 0.75 * bd.lineOpacity
            }
        }
    }
    // Faint scanlines over everything
    Column {
        anchors.fill: parent
        opacity: 0.18
        Repeater {
            model: Math.ceil(bd.height / Math.max(2, 4 * Theme.u))
            Rectangle {
                width: bd.width
                height: Math.max(1, Theme.u)
                color: "black"
                Rectangle { width: 1; height: Math.max(1, 3 * Theme.u) - 1; color: "transparent" }
            }
        }
        spacing: Math.max(1, 3 * Theme.u)
    }
}
