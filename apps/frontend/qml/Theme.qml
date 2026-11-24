// SPDX-License-Identifier: GPL-3.0-or-later
// Zen1 Arcade look: classic arcade + pixel art + futuristic neon, sized for
// reading from a cabinet's distance. Laid out for 1280x1024, scaled by u.
pragma Singleton
import QtQuick

QtObject {
    property real u: 1.0                 // window scale (1.0 on the 1280x1024 cabinet)
    readonly property color bg: "#05040f"
    readonly property color panel: "#0d0b24"
    readonly property color cyan: "#29e6ff"
    readonly property color pink: "#ff3cc8"
    readonly property color violet: "#8a5cff"
    readonly property color amber: "#ffc23c"
    readonly property color green: "#46ff9a"
    readonly property color red: "#ff4a5c"
    readonly property color text: "#f2f0ff"
    readonly property color dim: "#8c88b4"
    // Fonts (SIL OFL): blocky 8x8 arcade lettering, a terminal font for
    // details, and a softer pixel font for game titles.
    readonly property string pixel: "Press Start 2P"
    readonly property string mono: "VT323"
    readonly property string title: "Pixelify Sans"
    readonly property string display: title
    function px(v) { return Math.round(v * u) }
    // Press Start 2P is drawn on an 8-pixel grid: keep it on whole multiples.
    function pf(v) { return Math.max(8, Math.round(v * u / 8) * 8) }
}
