// SPDX-License-Identifier: GPL-3.0-or-later
// One game in the library grid: the user's artwork when present, otherwise a
// generated neon poster. Status is always visible: READY, ROMS NEEDED or NOT
// SUPPORTED (games whose hardware Zen1's engine does not implement).
import QtQuick
import Zen1 1.0

Item {
    id: card
    property var game: ({})
    property bool selected: false
    property real pulse: 0
    readonly property string status: game.status || ""
    readonly property bool ready: status === "ready"
    readonly property bool unsupported: status === "unsupported"
    readonly property color accent: ready ? Theme.cyan : unsupported ? Theme.red : Theme.amber

    scale: selected ? 1.07 : 1.0
    z: selected ? 2 : 1
    Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

    // Outer neon glow when selected
    Repeater {
        model: card.selected ? 5 : 0
        Rectangle {
            anchors.fill: face
            anchors.margins: -Theme.px(3 + index * 3)
            radius: face.radius + Theme.px(3 + index * 3)
            color: "transparent"
            border.color: card.accent
            border.width: Math.max(1, Theme.px(2))
            opacity: (0.5 - index * 0.09) * (0.75 + 0.25 * card.pulse)
        }
    }

    Rectangle {
        id: face
        anchors.fill: parent
        radius: Theme.px(14)
        clip: true
        color: Theme.panel
        border.color: card.selected ? card.accent : Qt.rgba(1, 1, 1, 0.10)
        border.width: card.selected ? Math.max(2, Theme.px(3)) : Math.max(1, Theme.px(1))
        gradient: Gradient {
            GradientStop { position: 0.0; color: card.unsupported ? "#14131c" : card.ready ? "#2a1268" : "#262033" }
            GradientStop { position: 1.0; color: card.unsupported ? "#0a0a10" : card.ready ? "#0a0b2a" : "#121018" }
        }

        // Generated poster (no artwork)
        Item {
            anchors.fill: parent
            visible: !art.visible
            opacity: card.unsupported ? 0.5 : 1
            // Diagonal neon stripes
            Repeater {
                model: 3
                Rectangle {
                    width: face.width * 1.8
                    height: Math.max(2, Theme.px(6 - index * 2))
                    x: -face.width * 0.4
                    y: face.height * (0.62 + index * 0.06)
                    rotation: -22
                    color: index === 1 ? Theme.pink : Theme.cyan
                    opacity: card.unsupported ? 0.15 : 0.55
                }
            }
            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.px(16)
                anchors.topMargin: Theme.px(52)
                text: card.game.title || ""
                wrapMode: Text.WordWrap
                maximumLineCount: 4
                elide: Text.ElideRight
                color: card.unsupported ? Theme.dim : Theme.text
                font.family: Theme.display
                font.pixelSize: Theme.px(card.unsupported ? 22 : 26)
                font.weight: Font.Black
                lineHeight: 0.95
            }
            Text {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: Theme.px(16)
                text: (card.game.year || "") + (card.game.system ? "\n" + card.game.system : "")
                color: card.unsupported ? Theme.dim : Theme.cyan
                font.pixelSize: Theme.px(17)
                font.bold: true
            }
        }
        Image {
            id: art
            anchors.fill: parent
            source: card.game.artwork || ""
            visible: status === Image.Ready
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: card.unsupported ? 0.35 : 1
        }
        // Shade under the badges so they read on any artwork
        Rectangle {
            width: parent.width
            height: Theme.px(46)
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.7) }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        // Status chip
        Rectangle {
            x: Theme.px(10)
            y: Theme.px(10)
            height: Theme.px(26)
            width: chip.implicitWidth + Theme.px(16)
            radius: height / 2
            color: Qt.rgba(0, 0, 0, 0.55)
            border.color: card.accent
            border.width: Math.max(1, Theme.px(2))
            Text {
                id: chip
                anchors.centerIn: parent
                text: card.ready ? "READY" : card.unsupported ? "NOT SUPPORTED" : "ROMS NEEDED"
                color: card.accent
                font.pixelSize: Theme.px(13)
                font.bold: true
                font.letterSpacing: Theme.px(1)
            }
        }
        Text {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.px(8)
            text: "★"
            visible: !!card.game.favourite
            color: Theme.amber
            font.pixelSize: Theme.px(30)
        }
        Rectangle {
            visible: (card.game.rotation || 0) === 90 || (card.game.rotation || 0) === 270
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.px(12)
            height: Theme.px(22)
            width: vert.implicitWidth + Theme.px(12)
            radius: Theme.px(4)
            color: Qt.rgba(0.54, 0.36, 1, 0.3)
            Text { id: vert; anchors.centerIn: parent; text: "VERTICAL"; color: Theme.text; font.pixelSize: Theme.px(12); font.bold: true }
        }
    }
}
