// SPDX-License-Identifier: GPL-3.0-or-later
// Settings: stick up/down picks a row, left/right changes it, LP runs an
// action, LK goes back. Changes are saved straight away.
import QtQuick
import Zen1 1.0

Item {
    id: page
    signal back()
    signal replayIntro()
    signal openControls()
    property int sel: 0
    readonly property var st: zen1.settings

    function onOff(b) { return b ? "ON" : "OFF" }
    readonly property var rows: [
        { label: "Startup intro", value: onOff(st.introEnabled), key: "intro.enabled", kind: "bool", cur: st.introEnabled },
        { label: "Intro length", value: st.introSeconds + " SECONDS", key: "intro.seconds", kind: "choice", cur: st.introSeconds, options: [4, 6, 8, 10] },
        { label: "Intro sound", value: onOff(st.introSound), key: "intro.sound", kind: "bool", cur: st.introSound },
        { label: "Any button skips the intro", value: onOff(st.introSkippable), key: "intro.skippable", kind: "bool", cur: st.introSkippable },
        { label: "Play the intro now", value: "▶", kind: "action", action: "intro" },
        { label: "Game picture", value: ({ "original": "ORIGINAL SHAPE", "integer": "INTEGER SCALE", "pixel-perfect": "PIXEL PERFECT", "stretch": "STRETCH (DISTORTS)" })[st.scale] || st.scale,
          key: "game.scale", kind: "choice", cur: st.scale, options: ["original", "integer", "pixel-perfect", "stretch"] },
        { label: "Screen surround", value: ({ "neon": "ZEN1 NEON", "black": "BLACK", "artwork": "MY BEZEL ARTWORK" })[st.bezel] || st.bezel,
          key: "game.bezel", kind: "choice", cur: st.bezel, options: ["neon", "black", "artwork"] },
        { label: "Frame timing", value: st.sync === "exact" ? "EXACT ORIGINAL SPEED" : "SMOOTHEST (FOLLOW SCREEN)",
          key: "game.sync", kind: "choice", cur: st.sync, options: ["display", "exact"] },
        { label: "Controllers", value: "▶", kind: "action", action: "controls" },
        { label: "Cabinet test card", value: "▶", kind: "action", action: "testcard" },
        { label: "Rescan the Games folder", value: "▶", kind: "action", action: "rescan" },
        { label: "Exit Zen1 Arcade", value: "▶", kind: "action", action: "quit" }
    ]

    function change(dir) {
        const r = rows[sel]
        if (r.kind === "bool") zen1.setSetting(r.key, !r.cur)
        else if (r.kind === "choice") {
            let i = r.options.indexOf(r.cur)
            i = (i + dir + r.options.length) % r.options.length
            zen1.setSetting(r.key, r.options[i])
        }
    }
    function activate() {
        const r = rows[sel]
        if (r.kind !== "action") { change(1); return }
        if (r.action === "intro") replayIntro()
        else if (r.action === "controls") openControls()
        else if (r.action === "testcard") zen1.launchTestCard()
        else if (r.action === "rescan") zen1.rescan()
        else if (r.action === "quit") zen1.quit()
    }
    function handle(name) {
        if (name === "up") sel = (sel + rows.length - 1) % rows.length
        else if (name === "down") sel = (sel + 1) % rows.length
        else if (name === "left") change(-1)
        else if (name === "right") change(1)
        else if (name === "lp" || name === "player") activate()
        else if (name === "lk") back()
    }

    Backdrop { anchors.fill: parent; horizon: 0.8; lineOpacity: 0.35 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.6 }

    NeonText { x: Theme.px(60); y: Theme.px(44); text: "SETTINGS"; size: Theme.px(56); color: "#eafcff"; glowColor: Theme.cyan; spacing: Theme.px(6) }

    Column {
        x: Theme.px(60)
        y: Theme.px(140)
        width: parent.width - 2 * x
        spacing: Theme.px(7)
        Repeater {
            model: page.rows
            Rectangle {
                readonly property bool on: index === page.sel
                width: parent.width
                height: Theme.px(52)
                radius: Theme.px(10)
                color: on ? Qt.rgba(0.16, 0.9, 1, 0.16) : Qt.rgba(1, 1, 1, 0.04)
                border.color: on ? Theme.cyan : "transparent"
                border.width: Math.max(1, Theme.px(2))
                Text {
                    x: Theme.px(22); anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label
                    color: modelData.action === "quit" ? Theme.red : Theme.text
                    font.pixelSize: Theme.px(24); font.bold: true
                }
                Text {
                    anchors.right: parent.right; anchors.rightMargin: Theme.px(22); anchors.verticalCenter: parent.verticalCenter
                    text: (parent.on && modelData.kind !== "action" ? "◀  " : "") + modelData.value + (parent.on && modelData.kind !== "action" ? "  ▶" : "")
                    color: modelData.kind === "action" ? Theme.pink : Theme.cyan
                    font.pixelSize: Theme.px(22); font.bold: true; font.letterSpacing: Theme.px(1)
                }
            }
        }
    }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: Theme.px(40)
        text: "Data folder  " + zen1.dataRoot + "\nScreen  " + zen1.screenInfo + "     Zen1 Arcade " + zen1.version
        color: Theme.dim; font.pixelSize: Theme.px(18); lineHeight: 1.3
    }
    Text {
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: Theme.px(40)
        text: "STICK  choose     LP  select     LK  back"
        color: Theme.dim; font.pixelSize: Theme.px(20); font.bold: true
    }
}
