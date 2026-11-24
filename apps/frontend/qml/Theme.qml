// SPDX-License-Identifier: GPL-3.0-or-later
// Zen1 Arcade look: classic arcade + futuristic neon, sized for reading from
// a cabinet's distance. Everything is laid out for 1280x1024 and scaled by u.
pragma Singleton
import QtQuick

QtObject {
    property real u: 1.0                 // window height / 1024
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
    readonly property string display: "Sans"   // headings: heavy, wide-spaced
    function px(v) { return Math.round(v * u) }
}
