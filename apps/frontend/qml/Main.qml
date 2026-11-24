// SPDX-License-Identifier: GPL-3.0-or-later
// Phase 2 frontend shell. The real intro and game grid arrive in Phase 5.
import QtQuick
import QtQuick.Window

Window {
    id: root
    visible: false          // shown fullscreen from C++ on the cabinet screen
    color: "#06060a"
    title: "Zen1 Arcade"

    readonly property color neon: "#00dcff"
    readonly property color pink: "#ff3caa"
    readonly property color dim: "#6e748c"

    Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
            // Development only; the cabinet never needs a keyboard.
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) zen1.launchTestCard()
            else if (event.key === Qt.Key_Escape) Qt.quit()
            else if (event.key === Qt.Key_F5) zen1.refreshControllers()
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: root.height * 0.02
        width: root.width * 0.8

        Text {
            id: title
            anchors.horizontalCenter: parent.horizontalCenter
            text: "ZEN1 ARCADE"
            color: root.neon
            font.pixelSize: root.height * 0.11
            font.bold: true
            font.letterSpacing: root.height * 0.012
            opacity: 0
            NumberAnimation on opacity { from: 0; to: 1; duration: 900; easing.type: Easing.OutCubic }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Phase 2 shell  ·  version " + zen1.version
            color: root.pink
            font.pixelSize: root.height * 0.026
        }

        Item { width: 1; height: root.height * 0.03 }

        Repeater {
            model: [
                ["Data folder", zen1.dataRoot],
                ["Screen", zen1.screenInfo],
                ["Player 1", zen1.player1],
                ["Player 2", zen1.player2]
            ]
            delegate: Row {
                spacing: root.width * 0.02
                Text {
                    width: root.width * 0.18
                    horizontalAlignment: Text.AlignRight
                    text: modelData[0]
                    color: root.dim
                    font.pixelSize: root.height * 0.028
                }
                Text {
                    width: root.width * 0.6
                    text: modelData[1]
                    elide: Text.ElideRight
                    color: "#ebebeb"
                    font.pixelSize: root.height * 0.028
                }
            }
        }

        Item { width: 1; height: root.height * 0.03 }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: startupError !== "" ? "Data folder problem: " + startupError
                : zen1.engineRunning ? "Test card running..."
                : "Press any Player 1 button to open the test card"
            color: startupError !== "" ? root.pink : "#ebebeb"
            font.pixelSize: root.height * 0.032
            SequentialAnimation on opacity {
                running: !zen1.engineRunning
                loops: Animation.Infinite
                NumberAnimation { to: 0.35; duration: 900 }
                NumberAnimation { to: 1.0; duration: 900 }
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: zen1.message
            visible: text !== ""
            color: root.pink
            font.pixelSize: root.height * 0.024
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Hold a button on both panels for 3 seconds to leave Zen1 Arcade"
            color: root.dim
            font.pixelSize: root.height * 0.022
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.width * 0.5 * zen1.exitProgress
            height: root.height * 0.006
            color: root.neon
            visible: zen1.exitProgress > 0
        }
    }
}
