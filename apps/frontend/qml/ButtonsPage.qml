// SPDX-License-Identifier: GPL-3.0-or-later
// One player's controls, one row per action: the four directions, six
// gameplay buttons, coin, start, the dedicated player button and the system
// functions. LP / 1P: press the new input for that row. MP: clear the row.
// Conflicts are shown in red; assigning an input that is already in use asks
// for a second press to confirm.
import QtQuick
import Zen1 1.0

Item {
    id: page
    property int player: 0
    signal back()
    property alias sel: menu.sel
    readonly property var map: player === 0 ? zen1.p1Mapping : zen1.p2Mapping
    readonly property bool assigned: player === 0 ? zen1.player1Assigned : zen1.player2Assigned

    function rowFor(m) {
        const v = m.value === "-" ? (m.note ? "(" + m.note.toUpperCase() + ")" : "NOT SET") : m.value + (m.note ? "  " + m.note.toUpperCase() : "")
        return { label: m.label + (m.conflict ? "  !" : ""), value: v, kind: "action", key: m.key, conflict: m.conflict,
                 danger: m.conflict !== "", run: function() { zen1.startBind(page.player, m.key) } }
    }
    readonly property var rows: {
        const r = [{ header: "PLAYER " + (player + 1) + "  STICK" }]
        for (let i = 0; i < map.length; ++i) {
            if (i === 4) r.push({ header: "GAMEPLAY BUTTONS" })
            if (i === 10) r.push({ header: "COIN AND START" })
            if (i === 12) r.push({ header: "SYSTEM (NEVER SENT TO THE GAME)" })
            r.push(rowFor(map[i]))
        }
        r.push({ header: "LAYOUT" })
        r.push({ label: "Quick setup: press 6 buttons + player button", value: ">", kind: "action", run: function() { zen1.startRemap(page.player) } })
        r.push({ label: "Reset Player " + (player + 1) + " to defaults", value: ">", kind: "action", danger: true, run: function() { zen1.resetMapping(page.player) } })
        r.push({ label: "Swap Player 1 and Player 2 layouts", value: ">", kind: "action", run: function() { zen1.swapMappings() } })
        return r
    }

    function handle(name) {
        if (zen1.captureActive) return
        if (name === "mp") {
            const r = rows[menu.sel]
            if (r && r.key) { zen1.sfx("back"); zen1.clearBind(page.player, r.key) }
            return
        }
        if (menu.handle(name)) return
        if (name === "lk") { zen1.sfx("back"); back() }
    }

    Backdrop { anchors.fill: parent; horizon: 0.86; lineOpacity: 0.3 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.7 }
    NeonText { x: Theme.px(56); y: Theme.px(36); text: "PLAYER " + (page.player + 1) + " BUTTONS"; size: Theme.px(44); color: "#eafcff"; glowColor: page.player === 0 ? Theme.cyan : Theme.pink }
    PixelText { anchors.right: parent.right; anchors.rightMargin: Theme.px(56); y: Theme.px(40); text: "LP SET   MP CLEAR   LK BACK"; size: 8; color: Theme.dim }
    PixelText {
        anchors.right: parent.right; anchors.rightMargin: Theme.px(56); y: Theme.px(64)
        text: page.assigned ? "" : "NO CONTROLLER FOR PLAYER " + (page.player + 1)
        size: 8; color: Theme.amber
    }

    MenuList {
        id: menu
        x: Theme.px(56); y: Theme.px(104)
        width: parent.width - 2 * x
        height: parent.height - y - Theme.px(86)
        rows: page.rows
    }

    Text {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: Theme.px(56); anchors.bottomMargin: Theme.px(22)
        wrapMode: Text.WordWrap
        font.family: Theme.mono; font.pixelSize: Theme.px(22)
        readonly property var cur: page.rows[menu.sel]
        color: cur && cur.conflict ? Theme.red : Theme.dim
        text: cur && cur.conflict ? "Also used for: " + cur.conflict
              : "The player button: press = coin + start, double press = volume, hold 5 s = library (1P) / restart (2P)."
    }

    CaptureOverlay { anchors.fill: parent }
}
