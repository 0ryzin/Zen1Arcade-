// SPDX-License-Identifier: GPL-3.0-or-later
// Dedicated 1P / 2P button gestures and the three-level cabinet volume.
#include "zen1/input/gesture.hpp"
#include "zen1_test.hpp"

#include <vector>

using namespace zen1::input;
using GE = ButtonGesture::Event;

namespace {
// Feeds a press/release timeline (seconds) sampled every 10 ms and returns
// every event that came out.
std::vector<GE> run(const std::vector<std::pair<double, double>>& presses, double until, double* maxProgress = nullptr) {
    ButtonGesture g;
    std::vector<GE> out;
    for (double t = 0; t <= until; t += 0.01) {
        bool down = false;
        for (const auto& [a, b] : presses) down = down || (t >= a && t < b);
        const auto r = g.update(down, t);
        if (r.event != GE::None) out.push_back(r.event);
        if (maxProgress) *maxProgress = std::max(*maxProgress, r.progress);
    }
    return out;
}
}  // namespace

TEST("Gesture: a single press is coin/start, reported once, after the double-press window") {
    ButtonGesture g;
    CHECK((g.update(true, 0.00).event == GE::None));
    CHECK((g.update(false, 0.15).event == GE::None));
    CHECK((g.update(false, 0.40).event == GE::None));  // still inside the window
    CHECK((g.update(false, 0.51).event == GE::Single));
    CHECK((g.update(false, 0.60).event == GE::None));
    CHECK((run({{0.0, 0.12}}, 2.0) == std::vector<GE>{GE::Single}));
}

TEST("Gesture: double press is volume only - no coin, no start, no hold") {
    CHECK((run({{0.0, 0.10}, {0.25, 0.35}}, 2.0) == std::vector<GE>{GE::Double}));
    // second press held for 6 s: still just the double, never a long hold
    CHECK((run({{0.0, 0.10}, {0.25, 6.5}}, 8.0) == std::vector<GE>{GE::Double}));
    // two presses too far apart are two single presses
    CHECK((run({{0.0, 0.10}, {0.9, 1.0}}, 2.0) == std::vector<GE>{GE::Single, GE::Single}));
}

TEST("Gesture: 5 s hold fires once with progress; releasing early cancels; no coin, no volume") {
    double p = 0;
    CHECK((run({{0.0, 7.0}}, 8.0, &p) == std::vector<GE>{GE::Long}));
    CHECK(p >= 1.0);
    p = 0;
    CHECK(run({{0.0, 4.5}}, 6.0, &p).empty());  // released at 4.5 s: cancelled, nothing else
    CHECK(p > 0.85 && p < 1.0);
    CHECK(run({{0.0, 1.5}}, 3.0).empty());      // a slow press is not a coin either
}

TEST("Gesture: switch bounce is not a double press") {
    // 1 press with 3 ms contact bounce on the way down and up
    CHECK((run({{0.0, 0.003}, {0.006, 0.12}, {0.123, 0.126}}, 1.0) == std::vector<GE>{GE::Single}));
}

TEST("Volume: exactly three levels, no wrap-around") {
    CHECK_EQ(volumeUp(0), 50);
    CHECK_EQ(volumeUp(50), 100);
    CHECK_EQ(volumeUp(100), 100);
    CHECK_EQ(volumeDown(100), 50);
    CHECK_EQ(volumeDown(50), 0);
    CHECK_EQ(volumeDown(0), 0);
    CHECK_EQ(volumePreset(70), 50);
    CHECK_EQ(volumePreset(90), 100);
    CHECK(volumeGain(0) == 0.0f);
    CHECK(volumeGain(50) > 0.2f && volumeGain(50) < 0.6f);
    CHECK(volumeGain(100) == 1.0f);
}

#include "zen1/emu/switch_package.hpp"

TEST("Switch packages: PFS0 file table read; title ID from the ticket or the file name") {
    // PFS0 with two files: a ticket and an NCA (no real data, headers only).
    std::vector<std::uint8_t> b(16 + 2 * 24, 0);
    auto le32 = [&](size_t o, std::uint32_t v) { for (int i = 0; i < 4; ++i) b[o + static_cast<size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i)); };
    const std::string names = std::string("01007330027EE0000000000000000000.tik") + '\0' + "abcdef.nca" + '\0';
    b[0] = 'P'; b[1] = 'F'; b[2] = 'S'; b[3] = '0';
    le32(4, 2);
    le32(8, static_cast<std::uint32_t>(names.size()));
    le32(16 + 16, 0);
    le32(16 + 24 + 16, 37);
    b.insert(b.end(), names.begin(), names.end());
    const auto files = zen1::emu::parsePfs0(b);
    CHECK(files.has_value());
    CHECK_EQ(files->size(), static_cast<size_t>(2));
    CHECK_EQ((*files)[1], std::string("abcdef.nca"));
    CHECK(!zen1::emu::parsePfs0(std::vector<std::uint8_t>(64, 0)).has_value());

    zen1::emu::SwitchPackage p;
    zen1::emu::parseSwitchFileName("Ultra Street Fighter II The Final Challengers [01007330027EE000][v0]", p);
    CHECK_EQ(p.name, std::string("Ultra Street Fighter II The Final Challengers"));
    CHECK_EQ(p.titleId, std::string("01007330027EE000"));
    CHECK_EQ(p.kind, std::string("Game"));
    CHECK_EQ(p.version, std::string("v0"));
}
