// SPDX-License-Identifier: GPL-3.0-or-later
// "Press a button" overlay for assignment and remapping. The prompt shows
// what was detected straight away. Cancel: Escape, or hold B on the Steam
// Deck. With no press at all it stops by itself after 60 seconds (backend).
import QtQuick
import Zen1 1.0

Rectangle {
    visible: zen1.captureActive
    color: Qt.rgba(0.01, 0, 0.05, 0.93)
    Column {
        anchors.centerIn: parent
        width: parent.width * 0.85
        spacing: Theme.px(26)
        NeonText { anchors.horizontalCenter: parent.horizontalCenter; text: "PRESS A BUTTON"; size: Theme.px(48); color: Theme.text; glowColor: Theme.pink }
        Text {
            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: zen1.capturePrompt
            font.family: Theme.mono; font.pixelSize: Theme.px(40); color: Theme.cyan
        }
        Text {
            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "Cancel: hold B on the Steam Deck, or Esc. Waits up to 60 seconds for a press."
            font.family: Theme.mono; font.pixelSize: Theme.px(26); color: Theme.dim
        }
    }
}
