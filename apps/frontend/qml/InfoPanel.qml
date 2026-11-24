// SPDX-License-Identifier: GPL-3.0-or-later
// The selected game: details on the left, the panel's controls for this
// game on the right (top row LP MP HP, bottom row LK MK HK, as on the
// cabinet), with the dedicated 1P / 2P buttons and what they do.
import QtQuick
import Zen1 1.0

Rectangle {
    id: info
    property var game: null
    property var map: zen1.p1Map    // panel button numbers for LP..HK + player
    readonly property bool unsupported: game !== null && game.status === "unsupported"
    readonly property var names: game && game.buttonNames ? game.buttonNames : []

    function players(g) {
        if (!g || !g.players) return "?"
        return g.players === 1 ? "1 PLAYER" : "1-" + g.players + " PLAYERS"
    }
    function controls(g) {
        if (!g || !g.joystick) return "?"
        return g.joystick.toUpperCase() + " JOYSTICK"
    }
    function buttons(g) {
        if (!g) return "?"
        const n = g.buttons !== undefined ? g.buttons : names.length
        if (n === 0) return "JOYSTICK ONLY"
        return n + (n === 1 ? " BUTTON" : " BUTTONS") + " PER PLAYER"
    }

    color: Qt.rgba(0.02, 0.015, 0.07, 0.92)
    border.color: unsupported ? Qt.rgba(1, 0.29, 0.36, 0.6) : Qt.rgba(0.16, 0.9, 1, 0.45)
    border.width: Math.max(2, Theme.px(2))

    // Corner brackets
    Repeater {
        model: 4
        Item {
            x: index % 2 ? info.width - Theme.px(22) : Theme.px(-2)
            y: index < 2 ? Theme.px(-2) : info.height - Theme.px(22)
            width: Theme.px(24); height: Theme.px(24)
            Rectangle { width: parent.width; height: Theme.px(4); y: index < 2 ? 0 : parent.height - height; color: Theme.pink }
            Rectangle { width: Theme.px(4); height: parent.height; x: index % 2 ? parent.width - width : 0; color: Theme.pink }
        }
    }

    // ---------------------------------------------------------------- details
    Text {
        id: title
        x: Theme.px(26); y: Theme.px(18)
        width: parent.width * 0.53
        text: info.game ? info.game.title : ""
        elide: Text.ElideRight
        color: Theme.text
        font.family: Theme.title
        font.pixelSize: Theme.px(36)
        font.weight: Font.Bold
    }
    Grid {
        x: Theme.px(26)
        anchors.top: title.bottom; anchors.topMargin: Theme.px(12)
        columns: 2
        columnSpacing: Theme.px(12)
        rowSpacing: Theme.px(8)
        visible: info.game !== null
        Repeater {
            model: info.game ? [
                ["YEAR", info.game.year || "?"],
                ["MAKER", info.game.manufacturer || "?"],
                ["HARDWARE", info.game.system || "?"],
                ["FORMAT", info.game.format || "ZIP ROM set"],
                ["PLAYERS", info.players(info.game)],
                ["CONTROLS", info.controls(info.game)],
                ["BUTTONS", info.buttons(info.game)],
                ["STATUS", info.unsupported ? "NOT SUPPORTED" : "SUPPORTED"]
            ] : []
            Item {
                width: Theme.px(318)
                height: Theme.px(26)
                PixelText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData[0]
                    size: 8
                    color: Theme.dim
                    width: Theme.px(100)
                }
                Text {
                    x: Theme.px(104)
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.px(210)
                    elide: Text.ElideRight
                    text: modelData[1]
                    font.family: Theme.mono
                    font.pixelSize: Theme.px(24)
                    color: modelData[0] !== "STATUS" ? Theme.cyan : info.unsupported ? Theme.red : Theme.green
                }
            }
        }
    }
    Text {
        x: Theme.px(26)
        anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.px(12)
        width: parent.width * 0.55
        visible: info.unsupported && info.game && !!info.game.reason
        text: info.game && info.game.reason ? info.game.reason : ""
        elide: Text.ElideRight
        font.family: Theme.mono
        font.pixelSize: Theme.px(22)
        color: Theme.red
    }

    // ---------------------------------------------------------------- controls
    Item {
        id: pad
        anchors.right: parent.right; anchors.rightMargin: Theme.px(22)
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.px(500); height: parent.height - Theme.px(28)

        PixelText {
            text: "CONTROLS"
            size: 8
            color: Theme.dim
        }
        Grid {
            y: Theme.px(20)
            columns: 3
            columnSpacing: Theme.px(8)
            rowSpacing: Theme.px(6)
            Repeater {
                model: 6
                Column {
                    width: Theme.px(96)
                    spacing: Theme.px(2)
                    readonly property bool used: index < info.names.length
                    ArcadeButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        diameter: Theme.px(44)
                        label: ["LP", "MP", "HP", "LK", "MK", "HK"][index]
                        color: index < 3 ? Theme.cyan : Theme.pink
                        used: parent.used
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: parent.used ? info.names[index] : "-"
                        font.family: Theme.mono
                        font.pixelSize: Theme.px(19)
                        color: parent.used ? Theme.text : "#4a475e"
                    }
                }
            }
        }
        // Dedicated player buttons
        Column {
            anchors.right: parent.right
            y: Theme.px(20)
            spacing: Theme.px(12)
            Repeater {
                model: [["1P", "START", "HOLD 5S: LIBRARY"], ["2P", "JOIN", "HOLD 5S: RESTART"]]
                Row {
                    spacing: Theme.px(8)
                    ArcadeButton {
                        diameter: Theme.px(40)
                        label: modelData[0]
                        color: Theme.amber
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        PixelText { text: modelData[1]; size: 8; color: Theme.amber }
                        Text { text: modelData[2]; font.family: Theme.mono; font.pixelSize: Theme.px(18); color: Theme.dim }
                    }
                }
            }
        }
        Text {
            anchors.bottom: parent.bottom
            width: parent.width
            text: info.game && info.names.length === 0 && !info.unsupported
                  ? "This game uses the joystick only (" + (info.game.joystick || "8-way") + ")"
                  : "Panel buttons " + info.map.slice(0, 6).map(function(i) { return i + 1 }).join(" ")
                    + "  |  1P button " + (info.map[6] + 1)
            font.family: Theme.mono
            font.pixelSize: Theme.px(18)
            color: Theme.dim
        }
    }
}
