// SPDX-License-Identifier: GPL-3.0-or-later
// SETUP: everything that is not choosing a game. Saved straight away.
import QtQuick
import Zen1 1.0

Item {
    id: page
    signal back()
    signal replayIntro()
    signal openControls()
    signal openTest()
    readonly property var st: zen1.settings
    property alias sel: menu.sel
    readonly property string selTitle: {
        for (let i = 0; i < zen1.games.length; ++i)
            if (zen1.games[i].set === zen1.selectedSet) return zen1.games[i].title
        return ""
    }

    function onOff(b) { return b ? "ON" : "OFF" }
    function handle(name) {
        if (menu.handle(name)) return
        if (name === "lk") { zen1.sfx("back"); back() }
    }

    Backdrop { anchors.fill: parent; horizon: 0.86; lineOpacity: 0.3 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.7 }

    NeonText { x: Theme.px(56); y: Theme.px(36); text: "SETUP"; size: Theme.px(48); color: "#eafcff"; glowColor: Theme.cyan }
    PixelText { anchors.right: parent.right; anchors.rightMargin: Theme.px(56); y: Theme.px(52); text: "STICK MOVE  LP SELECT  LK BACK"; size: 8; color: Theme.dim }

    MenuList {
        id: menu
        x: Theme.px(56); y: Theme.px(116)
        width: parent.width - 2 * x
        height: parent.height - y - Theme.px(90)
        rows: [
            { header: "CONTROLS" },
            { label: "Controllers: assign, swap, buttons, calibrate", value: ">", kind: "action", run: function() { page.openControls() } },
            { label: "Controller test", value: ">", kind: "action", run: function() { page.openTest() } },
            { header: "DISPLAY" },
            { label: "Game picture", key: "game.scale", kind: "choice", cur: page.st.scale, options: ["original", "integer", "pixel-perfect", "stretch"],
              value: ({ "original": "ORIGINAL SHAPE", "integer": "INTEGER", "pixel-perfect": "PIXEL PERFECT", "stretch": "STRETCH" })[page.st.scale] || page.st.scale },
            { label: "Screen surround", key: "game.bezel", kind: "choice", cur: page.st.bezel, options: ["neon", "black", "artwork"],
              value: ({ "neon": "ZEN1 NEON", "black": "BLACK", "artwork": "MY BEZELS" })[page.st.bezel] || page.st.bezel },
            { label: "Frame timing", key: "game.sync", kind: "choice", cur: page.st.sync, options: ["display", "exact"],
              value: page.st.sync === "exact" ? "EXACT SPEED" : "SMOOTHEST" },
            { header: "AUDIO" },
            { label: "Cabinet volume (1P / 2P double press)", key: "audio.volume", kind: "choice", cur: zen1.volume, options: [0, 50, 100],
              value: zen1.volume === 0 ? "0% MUTED" : zen1.volume === 50 ? "50% LOW" : "100% FULL" },
            { label: "Menu music", key: "music.enabled", kind: "bool", cur: page.st.musicEnabled, value: page.onOff(page.st.musicEnabled) },
            { label: "Menu music volume", key: "music.volume", kind: "choice", cur: page.st.musicVolume, options: [25, 40, 60, 80, 100], value: page.st.musicVolume + "%" },
            { label: "Menu sounds", key: "sound.menu", kind: "bool", cur: page.st.menuSounds, value: page.onOff(page.st.menuSounds) },
            { label: "Menu sound volume", key: "sound.menu_volume", kind: "choice", cur: page.st.menuVolume, options: [25, 50, 70, 100], value: page.st.menuVolume + "%" },
            { label: "Insert Coin sounds", key: "insertcoin.sound", kind: "bool", cur: page.st.insertCoinSound, value: page.onOff(page.st.insertCoinSound) },
            { label: "Intro sound", key: "intro.sound", kind: "bool", cur: page.st.introSound, value: page.onOff(page.st.introSound) },
            { header: "STARTUP" },
            { label: "Startup animation", key: "intro.enabled", kind: "bool", cur: page.st.introEnabled, value: page.onOff(page.st.introEnabled) },
            { label: "Animation length", key: "intro.seconds", kind: "choice", cur: page.st.introSeconds, options: [4, 6, 8, 10], value: page.st.introSeconds + " SEC" },
            { label: "Any button skips the animation", key: "intro.skippable", kind: "bool", cur: page.st.introSkippable, value: page.onOff(page.st.introSkippable) },
            { label: "Insert Coin screen", key: "insertcoin.enabled", kind: "bool", cur: page.st.insertCoin, value: page.onOff(page.st.insertCoin) },
            { label: "Play the startup now", value: ">", kind: "action", run: function() { page.replayIntro() } },
            { header: "LIBRARY" },
            { label: "Download artwork automatically", key: "artwork.download", kind: "bool", cur: page.st.artworkDownload, value: page.onOff(page.st.artworkDownload) },
            { label: "Download missing artwork now", value: ">", kind: "action", run: function() { zen1.downloadArtwork() } },
            { label: "Next artwork for " + (page.selTitle || "the selected game"), value: ">", kind: "action", run: function() { zen1.nextArtwork(zen1.selectedSet) } },
            { label: "Reset artwork for " + (page.selTitle || "the selected game"), value: ">", kind: "action", run: function() { zen1.resetArtwork(zen1.selectedSet) } },
            { label: "Rescan the Games folder", value: ">", kind: "action", run: function() { zen1.rescan() } },
            { header: "SYSTEM" },
            { label: "Cabinet test card", value: ">", kind: "action", run: function() { zen1.launchTestCard() } },
            { label: "Exit Zen1 Arcade", value: ">", kind: "action", danger: true, run: function() { zen1.quit() } }
        ]
    }

    Text {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: Theme.px(56); anchors.bottomMargin: Theme.px(24)
        elide: Text.ElideMiddle
        font.family: Theme.mono
        font.pixelSize: Theme.px(22)
        color: Theme.dim
        text: (zen1.artworkStatus !== "" ? zen1.artworkStatus + "     " : "") + "Data " + zen1.dataRoot + "   Screen " + zen1.screenInfo + "   v" + zen1.version
    }
}
