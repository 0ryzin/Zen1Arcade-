// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game library: five games per row, collections along the top, details
// and the button guide at the bottom. Player 1's stick and buttons drive it:
//   stick         move (up from the first row reaches the collections)
//   LP / PLAYER   play the selected game
//   MP            favourite on/off
//   HP            search
//   MK / HK       previous / next collection
//   LK            back: clear the search or the message
import QtQuick
import Zen1 1.0

Item {
    id: lib
    signal openSettings()

    property string zone: "grid"      // "grid" or "tabs"
    property int tab: 0
    property string query: ""
    property real pulse: 0
    readonly property var tabs: [
        { key: "all", label: "ALL GAMES" },
        { key: "fav", label: "FAVOURITES" },
        { key: "recent", label: "RECENT" },
        { key: "cps1", label: "CAPCOM CPS1" },
        { key: "unsupported", label: "NOT SUPPORTED" },
        { key: "settings", label: "SETTINGS" }
    ]
    readonly property var shown: filter(zen1.games, tabs[tab].key, query)
    readonly property var current: grid.currentIndex >= 0 && grid.currentIndex < shown.length ? shown[grid.currentIndex] : null

    function filter(all, key, q) {
        let out = []
        const ql = q.toLowerCase()
        for (let i = 0; i < all.length; ++i) {
            const g = all[i]
            if (key === "fav" && !g.favourite) continue
            if (key === "recent" && !(g.lastPlayed > 0)) continue
            if (key === "cps1" && g.system !== "Capcom CPS1") continue
            if (key === "unsupported" && g.status !== "unsupported") continue
            if (key === "settings") continue
            if (ql !== "" && (g.title || "").toLowerCase().indexOf(ql) < 0 && (g.set || "").indexOf(ql) !== 0) continue
            out.push(g)
        }
        if (key === "recent") out.sort(function(a, b) { return b.lastPlayed - a.lastPlayed })
        return out
    }
    function countFor(key) { return key === "settings" ? -1 : filter(zen1.games, key, "").length }

    function setTab(i) {
        tab = (i + tabs.length) % tabs.length
        grid.currentIndex = 0
        if (tabs[tab].key === "settings") zone = "tabs"
    }

    function handle(name) {
        if (search.open) { search.handle(name); return }
        if (zone === "tabs") {
            if (name === "left") setTab(tab - 1)
            else if (name === "right") setTab(tab + 1)
            else if (name === "lp" || name === "player" || name === "down") {
                if (tabs[tab].key === "settings") { if (name !== "down") openSettings() }
                else if (shown.length > 0) zone = "grid"
            } else if (name === "lk" && shown.length > 0) zone = "grid"
            else if (name === "mk") setTab(tab - 1)
            else if (name === "hk") setTab(tab + 1)
            return
        }
        switch (name) {
        case "left": grid.moveCurrentIndexLeft(); break
        case "right": grid.moveCurrentIndexRight(); break
        case "down": grid.moveCurrentIndexDown(); break
        case "up":
            if (grid.currentIndex < 5) zone = "tabs"
            else grid.moveCurrentIndexUp()
            break
        case "lp":
        case "player":
            if (current) zen1.launchGame(current.set)
            break
        case "mp": if (current) zen1.toggleFavourite(current.set); break
        case "hp": search.open = true; break
        case "mk": setTab(tab - 1); break
        case "hk": setTab(tab + 1); break
        case "lk":
            if (zen1.message !== "") zen1.clearMessage()
            else if (query !== "") query = ""
            else zone = "tabs"
            break
        }
    }

    onShownChanged: {
        if (grid.currentIndex >= shown.length) grid.currentIndex = Math.max(0, shown.length - 1)
        if (shown.length === 0 && tabs[tab].key !== "settings") zone = "tabs"
    }

    SequentialAnimation on pulse {
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: 900; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0; duration: 900; easing.type: Easing.InOutSine }
    }

    Backdrop {
        anchors.fill: parent
        horizon: 0.66
        lineOpacity: 0.55
        NumberAnimation on phase { from: 0; to: 1; duration: 5200; loops: Animation.Infinite }
    }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.45 }

    // ---------------------------------------------------------------- header
    Item {
        id: header
        width: parent.width
        height: Theme.px(104)
        Row {
            x: Theme.px(40)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(14)
            NeonText { text: "ZEN1"; size: Theme.px(58); color: "#eafcff"; glowColor: Theme.cyan; spacing: Theme.px(2) }
            NeonText {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.px(8)
                text: "ARCADE"; size: Theme.px(26); color: "#ffe6fa"; glowColor: Theme.pink; spacing: Theme.px(9)
            }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(40)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(18)
            Repeater {
                model: [[1, zen1.player1Assigned], [2, zen1.player2Assigned]]
                Rectangle {
                    height: Theme.px(40)
                    width: pl.implicitWidth + Theme.px(44)
                    radius: height / 2
                    color: Qt.rgba(0, 0, 0, 0.45)
                    border.color: modelData[1] ? Theme.green : Qt.rgba(1, 1, 1, 0.2)
                    border.width: Math.max(1, Theme.px(2))
                    Rectangle {
                        x: Theme.px(14)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.px(12); height: width; radius: width / 2
                        color: modelData[1] ? Theme.green : "#3a3850"
                    }
                    Text {
                        id: pl
                        x: Theme.px(32)
                        anchors.verticalCenter: parent.verticalCenter
                        text: "P" + modelData[0] + (modelData[1] ? " READY" : " NO PANEL")
                        color: modelData[1] ? Theme.text : Theme.dim
                        font.pixelSize: Theme.px(17)
                        font.bold: true
                    }
                }
            }
            Text {
                id: clock
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.dim
                font.pixelSize: Theme.px(22)
                font.bold: true
                text: Qt.formatTime(new Date(), "hh:mm")
                Timer { interval: 10000; running: true; repeat: true; onTriggered: clock.text = Qt.formatTime(new Date(), "hh:mm") }
            }
        }
    }

    // ---------------------------------------------------------------- tabs
    Row {
        id: tabRow
        x: Theme.px(40)
        y: header.height
        height: Theme.px(56)
        spacing: Theme.px(10)
        Repeater {
            model: lib.tabs
            Item {
                readonly property bool active: index === lib.tab
                readonly property bool focused: active && lib.zone === "tabs"
                height: tabRow.height
                width: tl.implicitWidth + Theme.px(30)
                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: Theme.px(6)
                    anchors.bottomMargin: Theme.px(6)
                    radius: Theme.px(8)
                    color: parent.focused ? Qt.rgba(0.16, 0.9, 1, 0.18) : "transparent"
                    border.color: parent.focused ? Theme.cyan : "transparent"
                    border.width: Math.max(1, Theme.px(2))
                }
                Text {
                    id: tl
                    anchors.centerIn: parent
                    readonly property int n: lib.countFor(modelData.key)
                    text: modelData.label + (n >= 0 ? "  " + n : "")
                    color: parent.active ? Theme.text : Theme.dim
                    font.pixelSize: Theme.px(19)
                    font.bold: true
                    font.letterSpacing: Theme.px(2)
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - Theme.px(20)
                    height: Math.max(2, Theme.px(3))
                    radius: height / 2
                    color: modelData.key === "unsupported" ? Theme.red : Theme.pink
                    visible: parent.active
                }
            }
        }
    }

    // ---------------------------------------------------------------- grid
    GridView {
        id: grid
        x: Theme.px(28)
        y: tabRow.y + tabRow.height + Theme.px(14)
        width: parent.width - 2 * x
        height: Theme.px(640)
        cellWidth: width / 5
        cellHeight: Theme.px(320)
        clip: true
        model: lib.shown
        interactive: false
        keyNavigationWraps: false
        highlightMoveDuration: 120
        preferredHighlightBegin: Theme.px(10)
        preferredHighlightEnd: height - Theme.px(10)
        highlightRangeMode: GridView.ApplyRange
        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight
            GameCard {
                anchors.centerIn: parent
                width: parent.width - Theme.px(26)
                height: parent.height - Theme.px(26)
                game: modelData
                selected: index === grid.currentIndex && lib.zone === "grid"
                pulse: lib.pulse
            }
        }
    }

    // Empty collection
    Column {
        anchors.horizontalCenter: grid.horizontalCenter
        y: grid.y + Theme.px(180)
        spacing: Theme.px(12)
        visible: lib.shown.length === 0
        NeonText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: lib.tabs[lib.tab].key === "settings" ? "SETTINGS" : lib.query !== "" ? "NO MATCHES" : "NOTHING HERE YET"
            size: Theme.px(44); color: Theme.text; glowColor: Theme.violet
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            color: Theme.dim
            font.pixelSize: Theme.px(22)
            text: {
                const k = lib.tabs[lib.tab].key
                if (k === "settings") return "Press LP to open settings, controllers and the test card"
                if (lib.query !== "") return "Nothing matches \"" + lib.query + "\". Press LK to clear the search."
                if (k === "fav") return "Press MP on a game to make it a favourite"
                if (k === "recent") return "Games you play appear here"
                if (k === "unsupported") return "Every game file in your Games folder is either supported or missing"
                return "Put ROM sets (.zip, current MAME versions) in\n" + zen1.gamesDir
            }
        }
    }

    // ---------------------------------------------------------------- details
    Rectangle {
        id: details
        x: Theme.px(28)
        width: parent.width - 2 * x
        y: grid.y + grid.height + Theme.px(16)
        height: parent.height - y - Theme.px(22)
        radius: Theme.px(16)
        color: Qt.rgba(0.03, 0.02, 0.1, 0.82)
        border.color: Qt.rgba(0.16, 0.9, 1, 0.35)
        border.width: Math.max(1, Theme.px(2))

        Column {
            x: Theme.px(26)
            y: Theme.px(18)
            width: parent.width * 0.6
            spacing: Theme.px(8)
            visible: lib.current !== null
            Text {
                width: parent.width
                text: lib.current ? lib.current.title : ""
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.display
                font.weight: Font.Black
                font.pixelSize: Theme.px(34)
            }
            Text {
                text: lib.current ? [lib.current.year, lib.current.manufacturer, lib.current.system,
                                     lib.current.supported ? (lib.current.players + " players") : ""]
                                    .filter(function(x) { return x }).join("   ·   ") : ""
                color: Theme.cyan
                font.pixelSize: Theme.px(20)
                font.bold: true
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                font.pixelSize: Theme.px(19)
                color: !lib.current ? Theme.dim : lib.current.status === "ready" ? Theme.green
                     : lib.current.status === "unsupported" ? Theme.red : Theme.amber
                text: !lib.current ? ""
                    : lib.current.status === "ready" ? "Ready to play  ·  played " + (lib.current.plays || 0) + " times"
                    : lib.current.status === "unsupported" ? "Not supported: " + lib.current.reason
                    : lib.current.status === "missing" ? "ROM set needed: put " + lib.current.set + ".zip (current MAME version) in the Games folder"
                    : "ROM set incomplete or a different version: " + (lib.current.missing || []).slice(0, 3).join(", ")
            }
        }

        // Button guide: the real panel layout, top row punches, bottom row kicks
        Grid {
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(24)
            anchors.verticalCenter: parent.verticalCenter
            columns: 3
            rowSpacing: Theme.px(10)
            columnSpacing: Theme.px(14)
            Repeater {
                model: [["LP", "PLAY", Theme.cyan], ["MP", "FAVOURITE", Theme.amber], ["HP", "SEARCH", Theme.violet],
                        ["LK", "BACK", Theme.dim], ["MK", "◀ LIST", Theme.pink], ["HK", "LIST ▶", Theme.pink]]
                Row {
                    spacing: Theme.px(8)
                    Rectangle {
                        width: Theme.px(40); height: width; radius: width / 2
                        color: Qt.rgba(0, 0, 0, 0.4)
                        border.color: modelData[2]
                        border.width: Math.max(2, Theme.px(3))
                        Text { anchors.centerIn: parent; text: modelData[0]; color: Theme.text; font.pixelSize: Theme.px(14); font.bold: true }
                    }
                    Text {
                        width: Theme.px(104)
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData[1]
                        color: Theme.text
                        font.pixelSize: Theme.px(16)
                        font.bold: true
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------- search
    Rectangle {
        id: search
        property bool open: false
        property int sel: 0
        readonly property var keys: "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789".split("").concat(["SPACE", "DEL", "CLEAR", "DONE"])
        readonly property int cols: 10
        anchors.fill: parent
        color: Qt.rgba(0.01, 0.0, 0.05, 0.88)
        visible: open

        function handle(name) {
            const n = keys.length
            if (name === "left") sel = (sel + n - 1) % n
            else if (name === "right") sel = (sel + 1) % n
            else if (name === "up") sel = (sel + n - cols) % n
            else if (name === "down") sel = (sel + cols) % n
            else if (name === "lp" || name === "player") {
                const k = keys[sel]
                if (k === "DONE") open = false
                else if (k === "DEL") lib.query = lib.query.slice(0, -1)
                else if (k === "CLEAR") lib.query = ""
                else if (k === "SPACE") lib.query += " "
                else lib.query += k
                grid.currentIndex = 0
            } else if (name === "lk") lib.query = lib.query.slice(0, -1)
            else if (name === "hp" || name === "mp") open = false
        }

        Column {
            anchors.centerIn: parent
            spacing: Theme.px(28)
            NeonText { anchors.horizontalCenter: parent.horizontalCenter; text: "SEARCH"; size: Theme.px(44); color: Theme.text; glowColor: Theme.violet; spacing: Theme.px(8) }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.px(760); height: Theme.px(72); radius: Theme.px(12)
                color: Qt.rgba(0, 0, 0, 0.5)
                border.color: Theme.cyan; border.width: Math.max(2, Theme.px(2))
                Text {
                    anchors.centerIn: parent
                    text: (lib.query === "" ? "" : lib.query) + (lib.pulse > 0.5 ? "▌" : " ")
                    color: Theme.text; font.pixelSize: Theme.px(40); font.bold: true; font.letterSpacing: Theme.px(4)
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: lib.shown.length + " match" + (lib.shown.length === 1 ? "" : "es")
                color: Theme.dim; font.pixelSize: Theme.px(20)
            }
            Grid {
                anchors.horizontalCenter: parent.horizontalCenter
                columns: search.cols
                spacing: Theme.px(10)
                Repeater {
                    model: search.keys
                    Rectangle {
                        readonly property bool wide: modelData.length > 1
                        width: Theme.px(wide ? 96 : 64); height: Theme.px(64); radius: Theme.px(10)
                        color: index === search.sel ? Qt.rgba(0.16, 0.9, 1, 0.25) : Qt.rgba(1, 1, 1, 0.05)
                        border.color: index === search.sel ? Theme.cyan : Qt.rgba(1, 1, 1, 0.15)
                        border.width: Math.max(1, Theme.px(index === search.sel ? 3 : 1))
                        Text { anchors.centerIn: parent; text: modelData; color: Theme.text; font.pixelSize: Theme.px(wide ? 17 : 26); font.bold: true }
                    }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "LP  type     LK  delete     HP  close"
                color: Theme.dim; font.pixelSize: Theme.px(20); font.bold: true
            }
        }
    }
}
