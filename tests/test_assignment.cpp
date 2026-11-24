// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/assignment.hpp"
#include "zen1/input/bindings.hpp"
#include "zen1_test.hpp"

#include <filesystem>
#include <unistd.h>

using namespace zen1::input;
namespace fs = std::filesystem;

namespace {
DeviceInfo pad(const std::string& name, const std::string& byPath, std::uint16_t vid = 0x0079,
               std::uint16_t pid = 0x0006, const std::string& uniq = "") {
    DeviceInfo d;
    d.name = name;
    d.byPath = byPath;
    d.vendor = vid;
    d.product = pid;
    d.uniq = uniq;
    d.kind = DeviceKind::Joystick;
    return d;
}
const std::string kPortA = "/dev/input/by-path/pci-0000:04:00.3-usb-0:1.1:1.0-event-joystick";
const std::string kPortB = "/dev/input/by-path/pci-0000:04:00.3-usb-0:1.2:1.0-event-joystick";
}  // namespace

TEST("Device names 'Player 1' and 'Player 2' are assigned regardless of order") {
    const std::vector<DeviceInfo> devs = {pad("Player 2", kPortB), pad("Player 1", kPortA)};
    const auto a = assignPlayers(devs, {});
    CHECK(a.complete());
    CHECK_EQ(*a.players[0].device, static_cast<size_t>(1));
    CHECK_EQ(*a.players[1].device, static_cast<size_t>(0));
    CHECK(a.players[0].reason == MatchReason::DeviceName);
}

TEST("Identical panels with no names are never assigned by detection order") {
    const std::vector<DeviceInfo> devs = {pad("DragonRise Inc. Generic USB Joystick", kPortA),
                                          pad("DragonRise Inc. Generic USB Joystick", kPortB)};
    const auto a = assignPlayers(devs, {});
    CHECK(!a.players[0].device.has_value());
    CHECK(!a.players[1].device.has_value());
}

TEST("Saved port binding survives the devices being detected in the other order") {
    std::array<DeviceIdentity, 2> saved;
    saved[0] = DeviceIdentity::from(pad("Generic", kPortB));
    saved[1] = DeviceIdentity::from(pad("Generic", kPortA));
    const std::vector<DeviceInfo> boot1 = {pad("Generic", kPortA), pad("Generic", kPortB)};
    const std::vector<DeviceInfo> boot2 = {pad("Generic", kPortB), pad("Generic", kPortA)};
    for (const auto& devs : {boot1, boot2}) {
        const auto a = assignPlayers(devs, saved);
        CHECK(a.complete());
        CHECK_EQ(devs[*a.players[0].device].byPath, kPortB);
        CHECK_EQ(devs[*a.players[1].device].byPath, kPortA);
        CHECK(a.players[0].reason == MatchReason::SavedPort);
    }
}

TEST("Saved serial follows a panel moved to another port") {
    std::array<DeviceIdentity, 2> saved;
    saved[0] = DeviceIdentity::from(pad("Pad", kPortA, 0x1234, 0x5678, "SER-P1"));
    const std::vector<DeviceInfo> devs = {pad("Pad", kPortB, 0x1234, 0x5678, "SER-P1")};
    const auto a = assignPlayers(devs, saved);
    CHECK(a.players[0].device.has_value());
    CHECK(a.players[0].reason == MatchReason::SavedSerial);
}

TEST("Two identical serial-less panels on new ports are not guessed") {
    std::array<DeviceIdentity, 2> saved;
    saved[0] = DeviceIdentity::from(pad("Generic", kPortA));
    saved[1] = DeviceIdentity::from(pad("Generic", kPortB));
    const std::vector<DeviceInfo> devs = {pad("Generic", "/dev/input/by-path/other-1"),
                                          pad("Generic", "/dev/input/by-path/other-2")};
    const auto a = assignPlayers(devs, saved);
    CHECK(!a.players[0].device.has_value());
    CHECK(!a.players[1].device.has_value());
}

TEST("Unique model on a new port is matched to its saved player") {
    std::array<DeviceIdentity, 2> saved;
    saved[0] = DeviceIdentity::from(pad("Pad A", kPortA, 0x1111, 0x0001));
    saved[1] = DeviceIdentity::from(pad("Pad B", kPortB, 0x2222, 0x0002));
    const std::vector<DeviceInfo> devs = {pad("Pad B", "/x/1", 0x2222, 0x0002),
                                          pad("Pad A", "/x/2", 0x1111, 0x0001)};
    const auto a = assignPlayers(devs, saved);
    CHECK(a.complete());
    CHECK_EQ(*a.players[0].device, static_cast<size_t>(1));
    CHECK(a.players[0].reason == MatchReason::SavedUniqueModel);
}

TEST("Virtual and Valve devices are never given to a player") {
    DeviceInfo virt = pad("Player 1", "", 0x045e, 0x028e);
    virt.virtualDevice = true;
    DeviceInfo deck = pad("Player 2", "", 0x28de, 0x1205);
    deck.valve = true;
    const auto a = assignPlayers({virt, deck}, {});
    CHECK(!a.players[0].device.has_value());
    CHECK(!a.players[1].device.has_value());
}

TEST("A device is never used for both players") {
    const std::vector<DeviceInfo> devs = {pad("Player 1", kPortA)};
    std::array<DeviceIdentity, 2> saved;
    saved[1] = DeviceIdentity::from(devs[0]);  // stale: saved as player 2
    const auto a = assignPlayers(devs, saved);
    const bool both = a.players[0].device && a.players[1].device;
    CHECK(!both);
}

TEST("Name matching accepts common spellings only") {
    CHECK_EQ(playerFromName("Player 1"), 1);
    CHECK_EQ(playerFromName("player2"), 2);
    CHECK_EQ(playerFromName("P1"), 1);
    CHECK_EQ(playerFromName("1P"), 1);
    CHECK_EQ(playerFromName("Player 12"), 0);
    CHECK_EQ(playerFromName("DragonRise Inc. Generic USB Joystick"), 0);
}

TEST("Bindings save and load through controls.toml") {
    const fs::path dir = fs::temp_directory_path() / ("zen1-bind-" + std::to_string(::getpid()));
    fs::create_directories(dir);
    const fs::path file = dir / "controls.toml";
    std::array<DeviceIdentity, 2> ids;
    ids[0] = DeviceIdentity::from(pad("Player 1", kPortA, 0x0079, 0x0006, "abc"));
    ids[1] = DeviceIdentity::from(pad("Player 2", kPortB));
    CHECK(savePlayerBindings(file, ids));
    const auto back = loadPlayerBindings(file);
    CHECK_EQ(back[0].byPath, kPortA);
    CHECK_EQ(back[0].uniq, std::string("abc"));
    CHECK_EQ(back[1].vendor, static_cast<std::uint16_t>(0x0079));
    CHECK_EQ(back[1].name, std::string("Player 2"));
    fs::remove_all(dir);
}

#include "zen1/input/evdev_panel.hpp"
#include <linux/input.h>

TEST("Panel button numbers follow SDL's Linux order") {
    std::vector<bool> bits(KEY_MAX, false);
    for (int c : {BTN_TRIGGER, BTN_THUMB, BTN_BASE, BTN_A, KEY_ENTER}) bits[static_cast<size_t>(c)] = true;
    const auto l = zen1::input::layoutFromKeyBits(bits);
    CHECK((l.buttonCodes == std::vector<int>{BTN_TRIGGER, BTN_THUMB, BTN_BASE, BTN_A, KEY_ENTER}));
    CHECK_EQ(l.buttonIndex(BTN_BASE), 2);
    CHECK_EQ(l.buttonIndex(KEY_ENTER), 4);
    CHECK_EQ(l.buttonIndex(BTN_B), -1);
}

TEST("Panel state: hat wins over axes; axes use the reported range") {
    zen1::input::PanelLayout l;
    l.buttonCodes = {BTN_TRIGGER};
    l.hasXY = true;
    l.hasHat = true;
    zen1::input::PanelEventState s;
    CHECK(s.apply(l, EV_ABS, ABS_X, 0));
    CHECK_EQ(s.dirX, -1);
    CHECK(!s.apply(l, EV_ABS, ABS_X, 10));
    CHECK(s.apply(l, EV_ABS, ABS_X, 128));
    CHECK_EQ(s.dirX, 0);
    s.apply(l, EV_ABS, ABS_HAT0Y, 1);
    CHECK_EQ(s.dirY, 1);
    CHECK(s.apply(l, EV_KEY, BTN_TRIGGER, 1));
    CHECK(s.buttons[0]);
    CHECK(!s.apply(l, EV_KEY, BTN_TRIGGER, 2));
}

TEST("Button maps default to 0..5 + 6 and round-trip through controls.toml") {
    const auto f = std::filesystem::temp_directory_path() / "zen1-btnmap-test.toml";
    std::filesystem::remove(f);
    auto m = zen1::input::loadButtonMaps(f);
    CHECK_EQ(m[1].player, 6);
    m[1].action = {7, 6, 5, 4, 3, 2};
    m[1].player = 1;
    CHECK(zen1::input::saveButtonMaps(f, m));
    CHECK((zen1::input::loadButtonMaps(f)[1] == m[1]));
    std::filesystem::remove(f);
}

#include "zen1/input/calibration.hpp"

TEST("Calibration: drift is re-centred, range rescaled, dead zone applied") {
    using namespace zen1::input;
    CalibrationRecorder r;
    r.beginCentre();
    for (int i = 0; i < 20; ++i) r.add(0.30, -0.10);  // a stick resting off-centre
    r.beginRange();
    for (double v : {-0.6, 0.30, 0.9}) r.add(v, 0.0);
    for (double v : {-1.0, 0.8}) r.add(0.3, v);
    CHECK(!r.looksDigital());
    const auto c = r.result(0.4);
    CHECK(c.calibrated && !c.digital);
    int dx = 9, dy = 9;
    stickDirection(c, 0.30, -0.10, dx, dy);  // the resting position now reads as centre
    CHECK_EQ(dx, 0);
    CHECK_EQ(dy, 0);
    stickDirection(c, 0.70, -0.10, dx, dy);  // 2/3 of the way right
    CHECK_EQ(dx, 1);
    stickDirection(StickCalibration{}, 0.40, 0.0, dx, dy);  // uncalibrated: half-way threshold
    CHECK_EQ(dx, 0);
}

TEST("Calibration: digital sticks are detected and left alone; 4-way and 2-way restriction") {
    using namespace zen1::input;
    CalibrationRecorder r;
    r.beginCentre();
    r.add(0.0, 0.0);
    r.beginRange();
    for (double v : {-1.0, 0.0, 1.0, -1.0, 1.0}) r.add(v, -v);
    CHECK(r.looksDigital());
    CHECK(r.result(0.3).digital);
    int dx = 1, dy = -1;
    restrictWays(4, dx, dy, 1, 0);  // was right, now up-right: up is new
    CHECK_EQ(dx, 0);
    CHECK_EQ(dy, -1);
    dx = 1; dy = -1;
    restrictWays(4, dx, dy, 0, -1);  // was up: right is new
    CHECK_EQ(dx, 1);
    CHECK_EQ(dy, 0);
    dx = -1; dy = 1;
    restrictWays(2, dx, dy, 0, 0);
    CHECK_EQ(dy, 0);
    CHECK_EQ(waysFromString("4-way"), 4);
}

TEST("Calibration saves per controller and resets") {
    using namespace zen1::input;
    const auto f = std::filesystem::temp_directory_path() / "zen1-cal-test.toml";
    std::filesystem::remove(f);
    DeviceInfo d;
    d.vendor = 0x0079; d.product = 0x0006; d.uniq = "ABC-1";
    const std::string key = calibrationKey(d);
    CHECK(key.find("0079_0006") != std::string::npos);
    StickCalibration c;
    c.calibrated = true; c.centerX = 0.25; c.deadzone = 0.35;
    CHECK(saveCalibration(f, key, c));
    CHECK((loadCalibration(f, key) == c));
    CHECK(!loadCalibration(f, "other").calibrated);
    CHECK(resetCalibration(f, key));
    CHECK(!loadCalibration(f, key).calibrated);
    std::filesystem::remove(f);
}
