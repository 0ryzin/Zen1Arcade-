// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zen1 Arcade startup intro. Everything is a function of `time` (ms), so
// the same timeline plays live, can be skipped, and can be recorded frame by
// frame. The timeline is designed for 6 s and scaled to `duration`; the
// synthesised sound (intro_audio.cpp) follows the same marks:
//   0.0  a scanline draws across the dark screen
//   0.5  the neon grid floor rises, horizon glow
//   1.45 Z  1.65 E  1.85 N  2.05 1   letters slam in with RGB split
//   2.3  impact: flash, shockwave, ARCADE expands, sparks rise
//   2.6  light sweep across the logo
//   3.4  INSERT COIN blinks
//   4.8  zoom through the logo into the library
import QtQuick
import Zen1 1.0

Item {
    id: intro
    property real time: 0          // ms since the intro started
    property real duration: 6000   // ms
    signal finished()

    readonly property real s: duration / 6000
    readonly property real t: time / s         // position on the 6 s design timeline
    readonly property real ww: width
    readonly property real hh: height
    property bool done: false
    onTChanged: if (!done && t >= 6000) { done = true; finished() }

    function clamp01(x) { return Math.max(0, Math.min(1, x)) }
    function seg(a, b) { return clamp01((t - a) / (b - a)) }
    function easeOutCubic(x) { return 1 - Math.pow(1 - x, 3) }
    function easeOutBack(x) { const c = 1.9; return 1 + (c + 1) * Math.pow(x - 1, 3) + c * Math.pow(x - 1, 2) }
    function easeInCubic(x) { return x * x * x }

    Rectangle { anchors.fill: parent; color: "black" }

    // The grid world rises from below and races toward the viewer.
    Backdrop {
        id: world
        width: intro.ww
        height: intro.hh
        y: intro.hh * 0.35 * (1 - easeOutCubic(seg(500, 1600)))
        opacity: seg(500, 1300) * (1 - seg(5500, 6000))
        phase: ((intro.t / 650) * (1 + 3 * easeInCubic(seg(4800, 6000)))) % 1
    }

    // 0.0 - 1.3 s: the scanline
    Item {
        anchors.fill: parent
        opacity: 1 - seg(1100, 1500)
        Repeater {
            model: 3
            Rectangle {
                anchors.centerIn: parent
                width: intro.ww * easeOutCubic(seg(80, 650))
                height: Math.max(1, Theme.px([3, 12, 30][index]))
                color: index === 0 ? "#e8fdff" : Theme.cyan
                opacity: [1.0, 0.35, 0.12][index]
            }
        }
    }

    // Shockwave + flash at the impact
    Rectangle {
        readonly property real k: seg(2300, 3000)
        anchors.centerIn: logo
        width: intro.ww * 1.6 * easeOutCubic(k)
        height: width * 0.42
        radius: height / 2
        color: "transparent"
        border.color: Theme.cyan
        border.width: Math.max(1, Theme.px(6) * (1 - k))
        opacity: k > 0 && k < 1 ? 1 - k : 0
    }

    // Rising sparks
    Repeater {
        model: 48
        Rectangle {
            readonly property real seed: (index * 7919 % 997) / 997
            readonly property real seed2: (index * 104729 % 991) / 991
            readonly property real age: (intro.t - 2300 - seed * 1200) / 1000
            visible: age > 0 && intro.t < 5600
            width: Math.max(2, Theme.px(2 + seed2 * 4))
            height: width
            radius: width / 2
            color: index % 3 === 0 ? Theme.cyan : index % 3 === 1 ? Theme.pink : Theme.amber
            x: intro.ww * (0.08 + 0.84 * seed) + Math.sin(age * 3 + index) * Theme.px(18)
            y: intro.hh * 0.58 - age * intro.hh * (0.18 + 0.25 * seed2)
            opacity: Math.max(0, 1 - age / 2.2) * (0.6 + 0.4 * Math.sin(age * 14 + index))
        }
    }

    // The logo
    Item {
        id: logo
        width: intro.ww
        height: intro.hh * 0.4
        y: intro.hh * 0.22
        readonly property real zoom: easeInCubic(seg(4800, 5800))
        scale: 1 + zoom * 5
        opacity: 1 - seg(5100, 5700)

        Row {
            id: letters
            anchors.horizontalCenter: parent.horizontalCenter
            y: 0
            spacing: Theme.px(6)
            Repeater {
                model: ["Z", "E", "N", "1"]
                Item {
                    id: letter
                    readonly property real at: 1450 + index * 200
                    readonly property real k: intro.seg(at, at + 220)
                    readonly property real sweep: intro.seg(2600 + index * 110, 2600 + index * 110 + 260)
                    readonly property real flash: Math.sin(sweep * Math.PI)
                    width: Theme.px(index === 3 ? 150 : 205)
                    height: Theme.px(250)
                    opacity: k > 0 ? 1 : 0
                    scale: 2.8 - 1.8 * intro.easeOutBack(k)
                    // RGB split copies converge as the letter lands
                    NeonText {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: -(1 - letter.k) * Theme.px(70) - Theme.px(3)
                        text: modelData; family: "Sans"; size: Theme.px(240); color: Theme.cyan; glow: 0
                        opacity: 0.55 * (1 - intro.seg(3000, 3400) * 0.6)
                    }
                    NeonText {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: (1 - letter.k) * Theme.px(70) + Theme.px(3)
                        text: modelData; family: "Sans"; size: Theme.px(240); color: Theme.pink; glow: 0
                        opacity: 0.55 * (1 - intro.seg(3000, 3400) * 0.6)
                    }
                    NeonText {
                        anchors.centerIn: parent
                        text: modelData
                        family: "Sans"
                        size: Theme.px(240)
                        color: Qt.rgba(0.88 + 0.12 * letter.flash, 0.97 + 0.03 * letter.flash, 1, 1)
                        glowColor: Theme.cyan
                        glow: 0.8 + 0.6 * letter.flash
                    }
                }
            }
        }
        NeonText {
            anchors.horizontalCenter: parent.horizontalCenter
            y: letters.height + Theme.px(6)
            text: "ARCADE"
            family: "Sans"
            size: Theme.px(64)
            color: "#ffe6fa"
            glowColor: Theme.pink
            spacing: Theme.px(28 + 90 * (1 - intro.easeOutCubic(intro.seg(2300, 3000))))
            opacity: intro.seg(2300, 2600)
        }
        // Underline bar that grows with the impact
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: letters.height + Theme.px(96)
            width: intro.ww * 0.55 * intro.easeOutCubic(intro.seg(2350, 2900))
            height: Math.max(2, Theme.px(4))
            radius: height / 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.3; color: Theme.cyan }
                GradientStop { position: 0.7; color: Theme.pink }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    // Impact flash
    Rectangle {
        anchors.fill: parent
        color: "#fff4fd"
        opacity: intro.t >= 2300 ? 0.75 * Math.exp(-(intro.t - 2300) / 110) : 0
    }

    NeonText {
        anchors.horizontalCenter: parent.horizontalCenter
        y: intro.hh * 0.78
        text: "INSERT COIN"
        size: Theme.px(40)
        spacing: Theme.px(4)
        color: Theme.amber
        glowColor: "#ff8a00"
        visible: intro.t > 3400 && intro.t < 4900
        opacity: Math.floor((intro.t - 3400) / 300) % 2 === 0 ? 1 : 0.15
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.px(20)
        text: "v" + zen1.version
        color: Theme.dim
        font.pixelSize: Theme.px(18)
        opacity: intro.seg(2600, 3200) * (1 - intro.seg(4800, 5200))
    }

    // Final fade to black, the library fades in over it
    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: intro.seg(5500, 6000)
    }
}
