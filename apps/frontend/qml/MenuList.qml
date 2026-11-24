// SPDX-License-Identifier: GPL-3.0-or-later
// A cabinet menu: stick up/down picks a row (section headers are skipped),
// left/right changes a setting, LP / 1P runs it. Rows:
//   { header: "DISPLAY" }
//   { label, value, kind: "bool"|"choice"|"action", key, cur, options, run, danger }
import QtQuick
import Zen1 1.0

Item {
    id: ml
    property var rows: []
    property int sel: 1
    readonly property int rowHeight: Theme.px(46)

    function selectable(i) { return i >= 0 && i < rows.length && !rows[i].header }
    function fixSel() { if (!selectable(sel)) step(1, false) }
    onRowsChanged: fixSel()
    function step(d, sound) {
        let i = sel
        for (let k = 0; k < rows.length; ++k) {
            i = (i + d + rows.length) % rows.length
            if (selectable(i)) break
        }
        sel = i
        list.positionViewAtIndex(i, ListView.Contain)
        if (sound !== false) zen1.sfx("move")
    }
    function change(dir) {
        const r = rows[sel]
        if (!r) return
        if (r.kind === "bool") zen1.setSetting(r.key, !r.cur)
        else if (r.kind === "choice") {
            let i = r.options.indexOf(r.cur)
            if (i < 0) i = 0
            i = (i + dir + r.options.length) % r.options.length
            zen1.setSetting(r.key, r.options[i])
        } else return
        zen1.sfx("move")
    }
    function activate() {
        const r = rows[sel]
        if (!r) return
        if (r.kind === "action") { zen1.sfx("select"); r.run() }
        else change(1)
    }
    function handle(name) {
        if (name === "up") step(-1)
        else if (name === "down") step(1)
        else if (name === "left") change(-1)
        else if (name === "right") change(1)
        else if (name === "lp" || name === "player") activate()
        else return false
        return true
    }

    ListView {
        id: list
        anchors.fill: parent
        model: ml.rows
        clip: true
        interactive: false
        spacing: Theme.px(6)
        highlightMoveDuration: 120
        delegate: Item {
            width: list.width
            height: modelData.header ? Theme.px(40) : ml.rowHeight
            readonly property bool on: index === ml.sel
            PixelText {
                visible: !!modelData.header
                anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.px(6)
                x: Theme.px(4)
                text: modelData.header || ""
                size: 16
                color: Theme.pink
            }
            Rectangle {
                visible: !modelData.header
                anchors.fill: parent
                color: parent.on ? Qt.rgba(0.16, 0.9, 1, 0.16) : Qt.rgba(1, 1, 1, 0.035)
                border.color: parent.on ? Theme.cyan : "transparent"
                border.width: Math.max(2, Theme.px(2))
                Rectangle {  // pixel cursor
                    visible: parent.parent.on
                    x: Theme.px(10); anchors.verticalCenter: parent.verticalCenter
                    width: Theme.px(10); height: Theme.px(10); color: Theme.amber
                }
                Text {
                    x: Theme.px(30); anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.55
                    elide: Text.ElideRight
                    text: modelData.label || ""
                    font.family: Theme.mono
                    font.pixelSize: Theme.px(30)
                    color: modelData.danger ? Theme.red : Theme.text
                }
                PixelText {
                    anchors.right: parent.right; anchors.rightMargin: Theme.px(18)
                    anchors.verticalCenter: parent.verticalCenter
                    text: (parent.parent.on && modelData.kind !== "action" ? "< " : "") + (modelData.value || "")
                          + (parent.parent.on && modelData.kind !== "action" ? " >" : "")
                    size: 16
                    color: modelData.kind === "action" ? Theme.pink : Theme.cyan
                }
            }
        }
    }
}
