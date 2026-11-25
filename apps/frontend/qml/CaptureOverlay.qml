// SPDX-License-Identifier: GPL-3.0-or-later
// "Press a button" overlay for assignment and remapping. The prompt shows
// what was detected straight away. Cancel: Escape, any Steam Deck button,
// or wait 20 seconds.
import QtQuick
import Zen1 1.0

Rectangle {
    visible: zen1.captureActive
    color: Qt.rgba(0.01, 0, 0.05, 0.93)
    Timer { interval: 20000; running: zen1.captureActive; onTriggered: zen1.cancelCapture() }
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
            text: "Cancel: Esc or any Steam Deck button. Cancels by itself after 20 seconds."
            font.family: Theme.mono; font.pixelSize: Theme.px(26); color: Theme.dim
        }
    }
}
