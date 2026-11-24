// SPDX-License-Identifier: GPL-3.0-or-later
// Joystick calibration, step by step, with the panel only:
//   1. let go of the stick (resting position, drift)
//   2. move it fully round (range)
//   3. dead zone: LP smaller, HP bigger
//   4. test every direction, then 1P saves (LK discards)
// Digital (switch) sticks are recognised and only tested.
import QtQuick
import Zen1 1.0

Item {
    id: page
    signal back()
    property int player: 0
    property var reached: []
    readonly property var cal: zen1.calibration
    readonly property var live: player === 0 ? zen1.p1Live : zen1.p2Live

    function start(p) { player = p; reached = []; zen1.calibrationBegin(p) }
    function handle(name) {
        const step = cal.step
        if (name === "lk") { zen1.sfx("back"); zen1.calibrationCancel(); back(); return }
        if (step === "intro" && (name === "lp" || name === "player")) { zen1.sfx("select"); zen1.calibrationStep() }
        else if (step === "adjust") {
            if (name === "lp") { zen1.calibrationDeadzone(-1); zen1.sfx("move") }
            else if (name === "hp") { zen1.calibrationDeadzone(1); zen1.sfx("move") }
            else if (name === "player" || name === "mp") { zen1.sfx("select"); zen1.calibrationStep() }
        } else if (step === "test" && name === "player") {
            zen1.sfx("coin"); zen1.calibrationSave(); back()
        }
    }
    onCalChanged: if (cal.step === "" && visible) back()

    Connections {
        target: zen1
        function onLiveChanged() {
            const l = page.live
            if (page.cal.step !== "test" || !l.connected || (!l.dirX && !l.dirY)) return
            const k = l.dirX + "," + l.dirY
            if (page.reached.indexOf(k) < 0) { page.reached = page.reached.concat([k]); zen1.sfx("move") }
        }
    }

    Backdrop { anchors.fill: parent; horizon: 0.9; lineOpacity: 0.25 }
    Rectangle { anchors.fill: parent; color: Theme.bg; opacity: 0.75 }
    NeonText { x: Theme.px(56); y: Theme.px(36); text: "CALIBRATE PLAYER " + (page.player + 1); size: Theme.px(40); color: "#eafcff"; glowColor: Theme.cyan }

    StickView {
        x: Theme.px(80); y: Theme.px(170)
        width: Theme.px(440); height: width
        live: page.live
        deadzone: page.cal.step === "adjust" || page.cal.step === "test" ? page.cal.deadzone : (page.live.deadzone || 0.5)
        reached: page.reached
    }

    Column {
        x: Theme.px(580); y: Theme.px(176)
        width: parent.width - x - Theme.px(60)
        spacing: Theme.px(22)
        PixelText {
            text: ({ "intro": "STEP 1 OF 4", "centre": "STEP 1 OF 4", "range": "STEP 2 OF 4", "adjust": "STEP 3 OF 4", "test": "STEP 4 OF 4" })[page.cal.step] || ""
            size: 16; color: Theme.pink
        }
        NeonText {
            width: parent.width
            text: ({ "intro": "READY?", "centre": "LET GO", "range": "MOVE IT ROUND", "adjust": "DEAD ZONE", "test": "TEST" })[page.cal.step] || ""
            size: Theme.px(40); color: Theme.text; glowColor: Theme.cyan
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            font.family: Theme.mono; font.pixelSize: Theme.px(30); color: Theme.text
            text: {
                const s = page.cal.step
                if (s === "intro") return "Calibrating " + (page.live.name || "this panel") + ".\nPress LP to start, LK to cancel."
                if (s === "centre") return "Leave the stick alone so its resting position can be measured. " + Math.max(0, 3 - page.cal.seconds).toFixed(1) + " s"
                if (s === "range") return "Push the stick all the way in every direction, slowly, round and round. " + Math.max(0, 5 - page.cal.seconds).toFixed(1) + " s"
                if (s === "adjust") return "Dead zone " + Math.round(page.cal.deadzone * 100) + "% (the pink circle). The stick counts as centred inside it.\nLP smaller   HP bigger   1P continue"
                if (s === "test") return (page.cal.digital ? "This is a digital stick (switches): software cannot and does not need to adjust it. " : "")
                                         + "Reach all eight directions (" + page.reached.length + "/8 so far).\n1P save   LK discard"
                return ""
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            visible: page.cal.step === "test" && !page.cal.digital
            font.family: Theme.mono; font.pixelSize: Theme.px(24); color: Theme.dim
            text: "Resting position " + page.cal.centerX.toFixed(2) + ", " + page.cal.centerY.toFixed(2)
                  + "  range x " + page.cal.minX.toFixed(2) + ".." + page.cal.maxX.toFixed(2)
                  + "  y " + page.cal.minY.toFixed(2) + ".." + page.cal.maxY.toFixed(2)
                  + "\nSoftware can re-centre and re-scale a stick; it cannot repair a worn or broken switch."
        }
    }
}
