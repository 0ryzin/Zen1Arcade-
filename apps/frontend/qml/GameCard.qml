// SPDX-License-Identifier: GPL-3.0-or-later
// One game in the library: its artwork first. Without artwork, a generated
// pixel-art marquee in Zen1 colours. Unsupported games are dimmed and say
// NOT SUPPORTED; supported games carry no status label at all.
import QtQuick
import Zen1 1.0

Item {
    id: card
    property var game: ({})
    property bool selected: false
    property real pulse: 0
    readonly property bool unsupported: (game.status || "") === "unsupported"
    readonly property color accent: unsupported ? Theme.red : Theme.cyan
    readonly property color hue: {
        const sys = game.system || ""
        if (sys.indexOf("CPS") >= 0) return "#2a1268"
        if (sys.indexOf("Pac-Man") >= 0) return "#3a2a00"
        return "#16142a"
    }

    scale: selected ? 1.06 : 1.0
    z: selected ? 2 : 1
    Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutBack } }

    // Neon selection frame (stepped, like pixels)
    Repeater {
        model: card.selected ? 4 : 0
        Rectangle {
            anchors.fill: face
            anchors.margins: -Theme.px(4 + index * 4)
            color: "transparent"
            border.color: index % 2 ? Theme.pink : card.accent
            border.width: Math.max(2, Theme.px(3))
            opacity: (0.9 - index * 0.22) * (0.7 + 0.3 * card.pulse)
        }
    }

    Rectangle {
        id: face
        anchors.fill: parent
        color: "#07060f"
        border.color: card.selected ? "white" : Qt.rgba(1, 1, 1, 0.12)
        border.width: Math.max(1, Theme.px(card.selected ? 2 : 1))
        clip: true

        // Generated marquee when there is no artwork
        Item {
            anchors.fill: parent
            visible: !art.visible
            opacity: card.unsupported ? 0.45 : 1
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: card.hue }
                    GradientStop { position: 1.0; color: "#05040f" }
                }
            }
            Rectangle {  // pixel sun
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height * 0.46
                width: parent.width * 0.5; height: width / 2
                color: card.unsupported ? "#3a3850" : Theme.pink
                clip: true
                Repeater {
                    model: 5
                    Rectangle { y: parent.height * (0.3 + index * 0.15); width: parent.width; height: Math.max(2, Theme.px(2 + index)); color: card.hue }
                }
            }
            Repeater {  // grid floor lines
                model: 6
                Rectangle {
                    y: face.height * (0.72 + index * index * 0.008)
                    width: face.width; height: Math.max(1, Theme.px(2))
                    color: card.unsupported ? "#3a3850" : Theme.cyan
                    opacity: 0.3 + index * 0.1
                }
            }
            Text {
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                anchors.margins: Theme.px(14); anchors.topMargin: Theme.px(18)
                text: (card.game.title || "").toUpperCase()
                wrapMode: Text.WordWrap
                maximumLineCount: 4
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                color: card.unsupported ? Theme.dim : Theme.text
                font.family: Theme.title
                font.pixelSize: Theme.px(28)
                font.weight: Font.Bold
                style: Text.Outline; styleColor: "black"
            }
            PixelText {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.px(14)
                text: card.game.year || ""
                size: 16
                color: card.unsupported ? Theme.dim : Theme.amber
            }
        }

        // Artwork: the whole image over a darkened, cropped copy of itself
        Image {
            anchors.fill: parent
            source: art.visible ? art.source : ""
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: 320
            asynchronous: true
            opacity: 0.35
        }
        Image {
            id: art
            anchors.fill: parent
            anchors.margins: Theme.px(4)
            source: card.game.artwork || ""
            visible: status === Image.Ready
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 400
            asynchronous: true
            opacity: card.unsupported ? 0.45 : 1
        }

        Rectangle {  // NOT SUPPORTED banner
            visible: card.unsupported
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.px(44)
            height: Theme.px(30)
            color: Qt.rgba(0.5, 0.02, 0.08, 0.9)
            PixelText { anchors.centerIn: parent; text: "NOT SUPPORTED"; size: 8; color: "white" }
        }
        Column {  // scanlines for a CRT feel
            anchors.fill: parent
            opacity: 0.12
            spacing: Math.max(1, Theme.px(2))
            Repeater {
                model: Math.ceil(face.height / Math.max(2, Theme.px(3)))
                Rectangle { width: face.width; height: Math.max(1, Theme.u); color: "black" }
            }
        }
    }
}
