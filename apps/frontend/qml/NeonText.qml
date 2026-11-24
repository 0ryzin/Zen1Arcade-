// SPDX-License-Identifier: GPL-3.0-or-later
// Text with a soft neon bloom, built from faint offset copies (works on any
// Qt Quick renderer, no shader effects needed).
import QtQuick
import Zen1 1.0

Item {
    id: nt
    property alias text: core.text
    property color color: Theme.cyan
    property color glowColor: color
    property real size: 40
    property real spacing: 0
    property real glow: 1.0
    property bool bold: true
    property int horizontalAlignment: Text.AlignLeft
    property string family: Theme.pixel
    readonly property bool isPixel: family === Theme.pixel
    implicitWidth: core.implicitWidth
    implicitHeight: core.implicitHeight
    width: implicitWidth
    height: implicitHeight

    Repeater {
        model: nt.glow > 0.01 ? 12 : 0
        Text {
            readonly property real ring: index < 6 ? 0.075 : 0.035
            readonly property real ang: (index % 6) * Math.PI / 3 + (index < 6 ? 0 : Math.PI / 6)
            x: core.x + Math.cos(ang) * nt.size * ring
            y: core.y + Math.sin(ang) * nt.size * ring
            width: core.width
            text: core.text
            font: core.font
            horizontalAlignment: core.horizontalAlignment
            color: nt.glowColor
            opacity: (index < 6 ? 0.10 : 0.20) * nt.glow
        }
    }
    Text {
        id: core
        width: nt.width
        color: nt.color
        horizontalAlignment: nt.horizontalAlignment
        font.family: nt.family
        font.pixelSize: nt.isPixel ? Math.max(8, Math.round(nt.size / 8) * 8) : Math.max(1, nt.size)
        font.bold: nt.bold && !nt.isPixel
        font.weight: nt.bold && !nt.isPixel ? Font.Black : Font.Normal
        renderType: nt.isPixel ? Text.NativeRendering : Text.QtRendering
        font.letterSpacing: nt.spacing
    }
}
