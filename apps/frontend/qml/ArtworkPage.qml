// SPDX-License-Identifier: GPL-3.0-or-later
// Choose your own artwork for one game with the stick: images from
// ~/Zen1Arcade/Artwork/import, Pictures, Downloads and Desktop, newest first.
import QtQuick
import Zen1 1.0

Item {
    id: page
    property string set: ""
    property string title: ""
    property var files: []
    signal back()
    function open(s, t) { set = s; title = t; files = zen1.imageFiles(); grid.currentIndex = 0 }

    function handle(name) {
        const cols = Math.max(1, Math.floor(grid.width / grid.cellWidth))
        if (name === "left") grid.currentIndex = Math.max(0, grid.currentIndex - 1)
        else if (name === "right") grid.currentIndex = Math.min(files.length - 1, grid.currentIndex + 1)
        else if (name === "up") grid.currentIndex = Math.max(0, grid.currentIndex - cols)
        else if (name === "down") grid.currentIndex = Math.min(files.length - 1, grid.currentIndex + cols)
        else if ((name === "lp" || name === "player") && files.length > 0) {
            zen1.sfx("select"); zen1.setCustomArtwork(set, files[grid.currentIndex].path); back(); return
        } else if (name === "lk") { zen1.sfx("back"); back(); return }
        else return
        zen1.sfx("move")
    }

    Backdrop { anchors.fill: parent; horizon: 0.86; lineOpacity: 0.3 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.75 }
    NeonText { x: Theme.px(56); y: Theme.px(30); text: "MY ARTWORK"; size: Theme.px(44); color: "#eafcff"; glowColor: Theme.cyan }
    Text {
        x: Theme.px(56); y: Theme.px(92); width: parent.width - 2 * x
        elide: Text.ElideRight
        text: page.title
        font.family: Theme.title; font.pixelSize: Theme.px(32); color: Theme.pink
    }
    PixelText { anchors.right: parent.right; anchors.rightMargin: Theme.px(56); y: Theme.px(44); text: "LP USE THIS   LK BACK"; size: 8; color: Theme.dim }

    GridView {
        id: grid
        x: Theme.px(56); y: Theme.px(150)
        width: parent.width - 2 * x
        height: parent.height - y - Theme.px(80)
        cellWidth: Theme.px(290); cellHeight: Theme.px(250)
        model: page.files
        clip: true
        interactive: false
        highlightMoveDuration: 100
        delegate: Item {
            width: grid.cellWidth; height: grid.cellHeight
            readonly property bool on: index === grid.currentIndex
            Rectangle {
                anchors.fill: parent; anchors.margins: Theme.px(8)
                color: Qt.rgba(1, 1, 1, 0.05)
                border.color: parent.on ? Theme.cyan : "#2a2838"
                border.width: Math.max(2, Theme.px(parent.on ? 4 : 2))
                Image {
                    anchors.fill: parent; anchors.margins: Theme.px(8); anchors.bottomMargin: Theme.px(40)
                    source: modelData.url
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: 400; sourceSize.height: 400
                }
                Text {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    anchors.margins: Theme.px(8)
                    elide: Text.ElideMiddle
                    text: modelData.name
                    font.family: Theme.mono; font.pixelSize: Theme.px(20); color: parent.parent.on ? Theme.text : Theme.dim
                }
            }
        }
    }
    Text {
        anchors.centerIn: parent
        width: parent.width * 0.7
        visible: page.files.length === 0
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        text: "No images found. Copy a PNG or JPG into " + zen1.dataRoot + "/Artwork/import (or Pictures / Downloads), then come back."
        font.family: Theme.mono; font.pixelSize: Theme.px(30); color: Theme.dim
    }
    Text {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: Theme.px(56); anchors.bottomMargin: Theme.px(26)
        elide: Text.ElideMiddle
        visible: page.files.length > 0
        text: page.files.length > 0 ? page.files[grid.currentIndex].folder + "/" + page.files[grid.currentIndex].name : ""
        font.family: Theme.mono; font.pixelSize: Theme.px(22); color: Theme.dim
    }
}
