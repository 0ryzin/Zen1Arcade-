// SPDX-License-Identifier: GPL-3.0-or-later
//
// The start-up scene: one continuous scene that plays the intro and then
// settles, without a cut, into the INSERT COIN idle state, where it waits
// for as long as it takes. A button press drops a coin, flashes PLAYER 1
// START and the library fades in over it.
//
// Everything is a function of `time` (ms since the scene started), so the
// same scene plays live, jumps to its settled state, and can be recorded
// frame by frame. The intro part is designed for 6 s and scaled to
// `duration`; its marks match the synthesised sound (intro_audio.cpp):
//   0.0  a scan beam wipes down and the dark atmosphere fades in
//   0.95 the logo's pixels stream in from the dark
//   1.45 Z  1.65 E  1.85 N  2.05 1  each letter's pixels lock into place
//   2.3  impact: light line, glow, ARCADE tracks in, frame brackets draw
//   3.0  3.15 two glitch jolts on the synth stabs
//   3.4  INSERT COIN starts blinking: the idle state from here on
// After `duration` the idle animations run on real time, with periods that
// all divide 600 s, so the live clock can wrap without a visible jump.
import QtQuick
import Zen1 1.0

Item {
    id: sc
    property real time: 0           // ms since the scene started
    property real duration: 6000    // ms of the intro part
    property bool coinEnabled: true // false: the intro ends in the library
    signal introFinished()
    signal done()                   // coin accepted, open the library

    property int credits: 0
    property bool starting: false
    property real startAt: 0        // time of the coin

    readonly property real s: duration / 6000
    readonly property real t: Math.min(time / s, 6000)   // design timeline, holds at the end
    readonly property real ww: width
    readonly property real hh: height
    readonly property bool settled: time >= duration
    property bool reported: false
    onTimeChanged: if (!reported && settled) { reported = true; introFinished() }

    function clamp01(x) { return Math.max(0, Math.min(1, x)) }
    function seg(a, b) { return clamp01((t - a) / (b - a)) }
    function easeOutCubic(x) { return 1 - Math.pow(1 - x, 3) }
    function easeOutQuart(x) { return 1 - Math.pow(1 - x, 4) }
    function frac(x) { return x - Math.floor(x) }
    function hash(n) { return frac(Math.sin(n * 127.1 + 311.7) * 43758.5453) }

    function reset() { starting = false; credits = 0; reported = false }
    function handle(name) {
        if (starting) return
        starting = true
        startAt = time
        credits = 1
        zen1.sfx("coin")
        startTimer.start()
    }
    Timer { id: startTimer; interval: 1200; onTriggered: sc.done() }
    Timer {  // a quiet attract bleep now and then while it waits
        interval: 12000; running: sc.visible && sc.settled && sc.coinEnabled && !sc.starting; repeat: true
        onTriggered: zen1.sfx("ambience")
    }

    // ---- Glitch: rows of the logo jump sideways for a moment ----------------
    // Two jolts in the intro (on the synth stabs) and a short one every 9 s
    // while idle.
    readonly property real glitch: {
        const j = (a) => (t >= a && t < a + 110) ? 1 - (t - a) / 110 : 0
        let g = Math.max(j(3000), j(3150))
        if (settled && !starting) {
            const p = time % 9000
            if (p > 8200 && p < 8320) g = Math.max(g, 0.6)
        }
        return g
    }
    readonly property real glitchSeed: Math.floor(time / 40)

    // ---- Atmosphere ---------------------------------------------------------
    Rectangle { anchors.fill: parent; color: "#020208" }

    Item {
        id: world
        anchors.fill: parent
        // The scan beam reveals it from the top.
        readonly property real reveal: sc.easeOutCubic(sc.seg(0, 900))
        opacity: 0.25 + 0.75 * sc.seg(150, 1400)

        // Faint perspective floor far below the logo.
        Backdrop {
            anchors.fill: parent
            horizon: 0.76
            gridColor: Theme.violet
            lineOpacity: 0.55
            opacity: 0.38
            phase: sc.frac(sc.time / 5000)
        }

        // Nebula light: soft colour clouds, painted once and drifted slowly.
        Canvas {
            id: nebula
            anchors.fill: parent
            anchors.margins: -Math.max(sc.ww, sc.hh) * 0.06
            readonly property real drift: Math.sin(2 * Math.PI * sc.time / 40000)
            x: drift * sc.ww * 0.02
            opacity: 0.85 + 0.15 * Math.sin(2 * Math.PI * sc.time / 12000)
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c = getContext("2d")
                c.reset()
                c.clearRect(0, 0, width, height)
                const blob = (cx, cy, r, col, a) => {
                    const g = c.createRadialGradient(cx, cy, 0, cx, cy, r)
                    g.addColorStop(0, Qt.rgba(col.r, col.g, col.b, a))
                    g.addColorStop(0.5, Qt.rgba(col.r, col.g, col.b, a * 0.35))
                    g.addColorStop(1, Qt.rgba(col.r, col.g, col.b, 0))
                    c.fillStyle = g
                    c.fillRect(0, 0, width, height)
                }
                const R = Math.max(width, height)
                blob(width * 0.22, height * 0.26, R * 0.48, Qt.rgba(0.04, 0.55, 0.70, 1), 0.22)
                blob(width * 0.82, height * 0.36, R * 0.50, Qt.rgba(0.42, 0.18, 0.85, 1), 0.24)
                blob(width * 0.50, height * 0.78, R * 0.45, Qt.rgba(0.85, 0.10, 0.60, 1), 0.14)
                blob(width * 0.50, height * 0.40, R * 0.30, Qt.rgba(0.20, 0.85, 1.0, 1), 0.08)
            }
        }

        // Dust: tiny square motes drifting upward through the light.
        Repeater {
            model: 46
            Rectangle {
                readonly property real sx: sc.hash(index + 1)
                readonly property real sy: sc.hash(index + 101)
                readonly property real period: [20000, 30000, 40000][index % 3]
                x: sc.ww * sx + Math.sin(2 * Math.PI * (sc.time / period + sy)) * Theme.px(16)
                y: sc.hh * (1 - sc.frac(sy + sc.time / period))
                width: Math.max(1, Theme.px(index % 7 === 0 ? 4 : 2)); height: width
                color: index % 5 === 0 ? Theme.pink : index % 3 === 0 ? Theme.violet : Theme.cyan
                opacity: (0.15 + 0.35 * Math.abs(Math.sin(2 * Math.PI * sc.time / 5000 + index)))
                         * sc.seg(600, 2000)
            }
        }

        // Vignette: darkens the edges so the centre glows.
        Canvas {
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c = getContext("2d")
                c.reset()
                const R = Math.sqrt(width * width + height * height) / 2
                const g = c.createRadialGradient(width / 2, height * 0.45, R * 0.35, width / 2, height * 0.45, R)
                g.addColorStop(0, "rgba(0,0,0,0)")
                g.addColorStop(1, "rgba(0,0,0,0.82)")
                c.fillStyle = g
                c.fillRect(0, 0, width, height)
            }
        }
    }
    // Everything below the beam stays dark until it has passed.
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right
        y: sc.hh * world.reveal
        height: sc.hh - y
        color: "#020208"
        visible: world.reveal < 1
    }

    // ---- Scan beam: the opening wipe, then a slow pass every 6 s ------------
    Item {
        id: beam
        readonly property real idlePos: sc.frac(sc.time / 6000)
        readonly property real pos: sc.t < 900 ? world.reveal : idlePos
        width: sc.ww
        height: Theme.px(90)
        y: sc.hh * pos - height
        opacity: sc.t < 900 ? 1 : (sc.settled ? 0.35 : sc.seg(1400, 3400) * 0.35)
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.85; color: Qt.rgba(0.16, 0.9, 1, 0.07) }
                GradientStop { position: 1.0; color: Qt.rgba(0.16, 0.9, 1, 0.20) }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width; height: Math.max(1, Theme.px(2))
            color: "#bff6ff"
            opacity: sc.t < 900 ? 0.9 : 0.25
        }
    }

    // ---- The logo: ZEN1 in glowing pixels -----------------------------------
    Item {
        id: logo
        readonly property var glyphs: [
            ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],   // Z
            ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],   // E
            ["#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#"],   // N
            ["..#..", ".##..", "#.#..", "..#..", "..#..", "..#..", "#####"]    // 1
        ]
        readonly property var cells: {
            let out = []
            for (let l = 0; l < 4; ++l)
                for (let r = 0; r < 7; ++r)
                    for (let c = 0; c < 5; ++c)
                        if (glyphs[l][r][c] === "#") out.push({ l: l, r: r, c: c })
            return out
        }
        readonly property real cell: Math.min(Theme.px(27), sc.ww * 0.026)
        readonly property real gap: Math.max(1, cell * 0.12)
        readonly property real letterGap: cell * 1.5
        width: 4 * 5 * cell + 3 * letterGap
        height: 7 * cell
        x: (sc.ww - width) / 2
        y: sc.hh * 0.24
        readonly property real impact: sc.t >= 2300 ? Math.exp(-(sc.t - 2300) / 380) : 0
        readonly property real breathe: sc.settled ? 0.5 + 0.5 * Math.sin(2 * Math.PI * sc.time / 4000) : 0.5 * sc.seg(2300, 3400)

        // Soft halo behind the whole word
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 1.25; height: parent.height * 1.9
            radius: height / 2
            color: Theme.cyan
            opacity: (0.035 + 0.025 * logo.breathe + 0.10 * logo.impact) * sc.seg(1900, 2400)
        }

        Repeater {
            model: logo.cells
            Item {
                id: px
                readonly property int letter: modelData.l
                readonly property int row: modelData.r
                readonly property real tx: letter * (5 * logo.cell + logo.letterGap) + modelData.c * logo.cell
                readonly property real ty: row * logo.cell
                // Each letter locks in on its sound mark; its pixels arrive
                // just before, from a scattered start, staggered a little.
                readonly property real seed: sc.hash(index * 3 + 7)
                readonly property real seed2: sc.hash(index * 5 + 11)
                readonly property real lands: 1450 + letter * 200 - seed * 140
                readonly property real k: sc.seg(lands - 520, lands)
                readonly property real ang: seed2 * Math.PI * 2
                readonly property real dist: sc.ww * (0.18 + 0.45 * seed)
                readonly property real fly: 1 - sc.easeOutQuart(k)
                // Glitch: a pseudo-random sideways jump for some rows.
                readonly property real rowJump: {
                    if (sc.glitch <= 0) return 0
                    const h = sc.hash(sc.glitchSeed * 13 + row * 7 + letter)
                    return h > 0.55 ? (h - 0.77) * logo.cell * 6 * sc.glitch : 0
                }
                x: tx + Math.cos(ang) * dist * fly + rowJump
                y: ty + Math.sin(ang) * dist * fly * 0.6
                width: logo.cell; height: logo.cell
                visible: k > 0
                opacity: 0.25 + 0.75 * k
                readonly property bool hit: Math.abs(rowJump) > 0.5

                Rectangle {   // glow
                    anchors.centerIn: parent
                    width: parent.width * 1.9; height: width
                    radius: width * 0.3
                    color: px.hit ? Theme.pink : Theme.cyan
                    opacity: (0.10 + 0.05 * logo.breathe + 0.25 * logo.impact) * px.k
                }
                Rectangle {   // the pixel
                    anchors.centerIn: parent
                    width: parent.width - logo.gap; height: width
                    color: px.hit ? "#ffd2f2"
                         : px.k < 1 ? Qt.rgba(0.35 + 0.6 * px.k, 0.85, 1, 1)
                         : Qt.rgba(0.86 + 0.14 * logo.impact, 0.98, 1, 1)
                    border.color: Qt.rgba(0.16, 0.9, 1, 0.9)
                    border.width: Math.max(1, logo.gap * 0.6)
                    // Bright edge on top: a lit, glassy pixel
                    Rectangle {
                        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                        anchors.margins: Math.max(1, logo.gap * 0.6)
                        height: parent.height * 0.28
                        color: "white"
                        opacity: 0.55
                    }
                }
            }
        }
    }

    // Light line through the logo at the impact
    Rectangle {
        readonly property real k: sc.seg(2300, 2900)
        anchors.horizontalCenter: parent.horizontalCenter
        y: logo.y + logo.height / 2 - height / 2
        width: sc.ww * 0.9 * sc.easeOutCubic(k)
        height: Math.max(2, Theme.px(3))
        visible: k > 0 && k < 1
        opacity: 1 - k
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: "white" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // ARCADE, tracking in from wide
    NeonText {
        anchors.horizontalCenter: parent.horizontalCenter
        y: logo.y + logo.height + Theme.px(34)
        text: "ARCADE"
        family: Theme.title
        size: Theme.px(46)
        spacing: Theme.px(26 + 70 * (1 - sc.easeOutCubic(sc.seg(2300, 3100))))
        color: "#ffe9fb"
        glowColor: Theme.pink
        glow: 0.6 + 0.3 * logo.breathe
        opacity: sc.seg(2300, 2700)
    }
    // A thin rule under it
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: logo.y + logo.height + Theme.px(108)
        width: logo.width * 0.9 * sc.easeOutCubic(sc.seg(2400, 3200))
        height: Math.max(1, Theme.px(2))
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.3; color: Theme.cyan }
            GradientStop { position: 0.7; color: Theme.violet }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Frame brackets around the logo, drawn after the impact
    Repeater {
        model: 4
        Item {
            readonly property bool right: index % 2 === 1
            readonly property bool bottom: index >= 2
            readonly property real k: sc.easeOutCubic(sc.seg(2400 + index * 60, 3000 + index * 60))
            readonly property real len: Theme.px(46) * k
            readonly property real th: Math.max(1, Theme.px(2))
            readonly property real pad: Theme.px(46)
            x: right ? logo.x + logo.width + pad : logo.x - pad
            y: bottom ? logo.y + logo.height + Theme.px(130) : logo.y - pad
            opacity: 0.75 * k
            Rectangle { x: parent.right ? -parent.len : 0; y: parent.bottom ? -parent.th : 0; width: parent.len; height: parent.th; color: Theme.cyan }
            Rectangle { x: parent.right ? -parent.th : 0; y: parent.bottom ? -parent.len : 0; width: parent.th; height: parent.len; color: Theme.cyan }
        }
    }

    // ---- The idle state ------------------------------------------------------
    readonly property real idleIn: sc.seg(3400, 3900)

    NeonText {
        id: coinText
        anchors.horizontalCenter: parent.horizontalCenter
        y: sc.hh * 0.68
        visible: sc.coinEnabled
        text: sc.starting ? "PLAYER 1 START" : "INSERT COIN"
        size: Theme.px(48)
        spacing: Theme.px(6)
        color: sc.starting ? "#eafcff" : Theme.amber
        glowColor: sc.starting ? Theme.cyan : "#ff8a00"
        opacity: sc.starting ? (Math.floor((sc.time - sc.startAt) / 90) % 2 ? 1 : 0.35)
                             : sc.idleIn * ((sc.time - 3400 * sc.s) % 1000 < 640 ? 1 : 0.06)
    }
    PixelText {
        anchors.horizontalCenter: parent.horizontalCenter
        y: coinText.y + coinText.height + Theme.px(26)
        visible: sc.coinEnabled && !sc.starting
        text: "PRESS ANY BUTTON"
        size: 16
        color: Theme.dim
        opacity: sc.idleIn * (0.5 + 0.35 * Math.sin(2 * Math.PI * sc.time / 3000))
    }

    // Cabinet bottom line
    PixelText {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        anchors.margins: Theme.px(34)
        visible: sc.coinEnabled
        text: "CREDIT " + (sc.credits < 10 ? "0" : "") + sc.credits
        size: 16
        color: sc.credits > 0 ? Theme.green : Theme.text
        opacity: sc.idleIn * 0.85
    }
    PixelText {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: Theme.px(34)
        visible: sc.coinEnabled
        text: "FREE PLAY"
        size: 16
        color: Theme.pink
        opacity: sc.idleIn * 0.85
    }
    Text {
        anchors.left: parent.left; anchors.top: parent.top
        anchors.margins: Theme.px(26)
        text: "ZEN1 // v" + zen1.version
        color: Theme.dim
        font.family: Theme.mono
        font.pixelSize: Theme.px(22)
        opacity: 0.7 * sc.seg(2600, 3200)
    }

    // Fine scanlines over everything
    Column {
        anchors.fill: parent
        opacity: 0.12
        spacing: Math.max(1, Theme.px(3))
        Repeater {
            model: Math.ceil(sc.height / Math.max(2, Theme.px(4)))
            Rectangle { width: sc.width; height: Math.max(1, Theme.u); color: "black" }
        }
    }

    // Flashes: the impact, and the coin
    Rectangle {
        anchors.fill: parent
        color: "#eafcff"
        opacity: {
            let o = sc.t >= 2300 ? 0.38 * Math.exp(-(sc.t - 2300) / 120) : 0
            if (sc.starting) o = Math.max(o, 0.55 * Math.exp(-(sc.time - sc.startAt) / 160))
            return o
        }
    }
}
