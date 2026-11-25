#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# End-to-end input test helper (CI). Creates kernel input devices with
# uinput that stand in for two arcade panels, and optionally a "Steam
# Virtual Gamepad" that mirrors every press the way Steam does in Game
# Mode, then presses buttons on a timeline. Zen1 reads them through the
# same /dev/input path as real panels (they are uinput devices, so this is
# not a test of physical USB hardware).
#
# usage (as root, so the devices can be made readable):
#   e2e_input.py TIMELINE
# TIMELINE: comma-separated "SECONDS:DEVICE:ACTION", e.g.
#   5:p1:trigger   press and release button 0 (BTN_TRIGGER) on panel 1
#   6:p1:b6        button 6 (the default player button)
#   7:p1:left/1.5  hold the stick left for 1.5 s
# DEVICE: p1, p2, or mirror (every p1/p2 press also goes to the Steam pad
# first when "mirror" is in the timeline's devices, see --mirror).
import sys
import time

import evdev
from evdev import UInput, ecodes as e

BUTTONS = [e.BTN_TRIGGER, e.BTN_THUMB, e.BTN_THUMB2, e.BTN_TOP, e.BTN_TOP2, e.BTN_PINKIE, e.BTN_BASE, e.BTN_BASE2, e.BTN_BASE3]


def panel(n):
    caps = {e.EV_KEY: BUTTONS, e.EV_ABS: [(e.ABS_X, evdev.AbsInfo(0, -127, 127, 0, 0, 0)),
                                          (e.ABS_Y, evdev.AbsInfo(0, -127, 127, 0, 0, 0))]}
    return UInput(caps, name=f"Zen1 Test Panel {n}", vendor=0x16C0, product=0x05E0 + n, version=0x111,
                  bustype=e.BUS_USB, phys=f"usb-zen1-test-{n}/input0")


def steam_pad():
    caps = {e.EV_KEY: [e.BTN_SOUTH, e.BTN_EAST, e.BTN_NORTH, e.BTN_WEST, e.BTN_START, e.BTN_SELECT],
            e.EV_ABS: [(e.ABS_X, evdev.AbsInfo(0, -32768, 32767, 0, 0, 0)),
                       (e.ABS_Y, evdev.AbsInfo(0, -32768, 32767, 0, 0, 0))]}
    return UInput(caps, name="Steam Virtual Gamepad", vendor=0x28DE, product=0x11FF, bustype=e.BUS_USB)


def main():
    timeline = []
    for item in sys.argv[1].split(","):
        t, dev, action = item.split(":")
        timeline.append((float(t), dev, action))
    mirror = "--mirror" in sys.argv
    devs = {"p1": panel(1), "p2": panel(2)}
    if mirror:
        devs["mirror"] = steam_pad()
    time.sleep(0.5)
    import os
    # Make the new event nodes readable by the test user.
    for path in evdev.list_devices():
        try:
            name = evdev.InputDevice(path).name
        except OSError:
            continue
        if name.startswith("Zen1 Test") or name == "Steam Virtual Gamepad":
            os.chmod(path, 0o666)
            print(f"e2e-input: {name} at {path}", flush=True)
    start = time.time()
    for t, dev, action in sorted(timeline):
        while time.time() - start < t:
            time.sleep(0.01)
        hold = 0.12
        if "/" in action:
            action, h = action.split("/")
            hold = float(h)
        ui = devs[dev]
        if mirror and dev in ("p1", "p2"):
            # Steam's mirror reports the same press a moment before the
            # real device does (the worst case for the old cancel rule).
            devs["mirror"].write(e.EV_KEY, e.BTN_SOUTH, 1)
            devs["mirror"].syn()
            time.sleep(0.005)
        if action in ("left", "right", "up", "down"):
            code = e.ABS_X if action in ("left", "right") else e.ABS_Y
            ui.write(e.EV_ABS, code, -127 if action in ("left", "up") else 127)
            ui.syn()
            time.sleep(hold)
            ui.write(e.EV_ABS, code, 0)
            ui.syn()
        else:
            code = e.BTN_TRIGGER if action == "trigger" else BUTTONS[int(action[1:])]
            ui.write(e.EV_KEY, code, 1)
            ui.syn()
            time.sleep(hold)
            ui.write(e.EV_KEY, code, 0)
            ui.syn()
        if mirror and dev in ("p1", "p2"):
            devs["mirror"].write(e.EV_KEY, e.BTN_SOUTH, 0)
            devs["mirror"].syn()
        print(f"e2e-input: {t:.1f}s {dev} {action}", flush=True)
    time.sleep(2)
    for d in devs.values():
        d.close()


main()
