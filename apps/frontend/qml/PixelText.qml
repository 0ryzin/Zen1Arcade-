// SPDX-License-Identifier: GPL-3.0-or-later
// Crisp pixel lettering (no smoothing, sizes on the font's own grid).
import QtQuick
import Zen1 1.0

Text {
    property int size: 16
    property bool mono: false   // the taller terminal font for details
    font.family: mono ? Theme.mono : Theme.pixel
    font.pixelSize: mono ? Theme.px(size) : Theme.pf(size)
    renderType: Text.NativeRendering
    color: Theme.text
}
