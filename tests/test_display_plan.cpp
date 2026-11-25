// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/display_plan.hpp"
#include "zen1_test.hpp"

#include <cmath>
#include <vector>

using namespace zen1;

namespace {
DisplayPlan plan(GameVideo v, int sw, int sh, ScaleMode m = ScaleMode::Original,
                 Rotation r = Rotation::R0, double zoom = 1.0) {
    PlanRequest q;
    q.video = v;
    q.screenWidth = sw;
    q.screenHeight = sh;
    q.mode = m;
    q.rotation = r;
    q.customZoom = zoom;
    return *computeDisplayPlan(q);
}
const GameVideo kCps1{384, 224, 4.0 / 3.0};
}  // namespace

TEST("SF2 on 1280x1024 fills the width at 4:3 with 32-pixel bands") {
    const auto p = plan(kCps1, 1280, 1024);
    CHECK_EQ(p.game, (Rect{0, 32, 1280, 960}));
}

TEST("SF2 integer scaling on 1280x1024 is 4x lines") {
    const auto p = plan(kCps1, 1280, 1024, ScaleMode::Integer);
    CHECK_EQ(p.integerFactor, 4);
    CHECK_EQ(p.game.h, 896);
    CHECK_EQ(p.game.w, 1195);
    CHECK_EQ(p.game.y, 64);
}

TEST("Vertical game rotated 90 degrees fills the height at 3:4") {
    const auto p = plan(kCps1, 1280, 1024, ScaleMode::Original, Rotation::R90);
    CHECK_EQ(p.game, (Rect{256, 0, 768, 1024}));
}

TEST("16:9 picture on 5:4 leaves 152-pixel bands, never stretched") {
    const auto p = plan({1920, 1080, 16.0 / 9.0}, 1280, 1024);
    CHECK_EQ(p.game, (Rect{0, 152, 1280, 720}));
}

TEST("Stretch fills the screen only when chosen") {
    const auto p = plan(kCps1, 1280, 1024, ScaleMode::Stretch);
    CHECK_EQ(p.game, (Rect{0, 0, 1280, 1024}));
}

TEST("Pixel-perfect uses square pixels") {
    const auto p = plan(kCps1, 1280, 1024, ScaleMode::PixelPerfect);
    CHECK_EQ(p.integerFactor, 3);
    CHECK_EQ(p.game, (Rect{64, 176, 1152, 672}));
}

TEST("Integer mode falls back when the screen is smaller than one copy") {
    const auto p = plan(kCps1, 320, 200, ScaleMode::Integer);
    CHECK(p.fallback);
    CHECK(p.game.w <= 320 && p.game.h <= 200);
}

TEST("Custom zoom shrinks around the centre") {
    const auto p = plan(kCps1, 1280, 1024, ScaleMode::Custom, Rotation::R0, 0.5);
    CHECK_EQ(p.game, (Rect{320, 272, 640, 480}));
}

TEST("Game rectangle always stays inside the screen") {
    for (int sw : {640, 800, 1280, 1920, 2560})
        for (int sh : {480, 720, 1024, 1080, 1440})
            for (auto m : {ScaleMode::Original, ScaleMode::Integer, ScaleMode::PixelPerfect})
                for (auto r : {Rotation::R0, Rotation::R90}) {
                    const auto p = plan(kCps1, sw, sh, m, r);
                    CHECK(p.game.x >= 0 && p.game.y >= 0);
                    CHECK(p.game.x + p.game.w <= sw && p.game.y + p.game.h <= sh);
                }
}

TEST("Invalid input gives no plan") {
    PlanRequest q;
    CHECK(!computeDisplayPlan(q).has_value());
}

TEST("Scale mode and rotation names round-trip") {
    for (auto m : {ScaleMode::Original, ScaleMode::Integer, ScaleMode::PixelPerfect,
                   ScaleMode::Stretch, ScaleMode::Custom})
        CHECK(scaleModeFromString(toString(m)) == m);
    CHECK(rotationFromDegrees(-90) == Rotation::R270);
    CHECK(!rotationFromDegrees(45).has_value());
}

TEST("Content tracker: black borders are found, the HUD is kept, the area only grows") {
    const int w = 320, h = 224;
    std::vector<std::uint32_t> f(static_cast<size_t>(w * h), 0);
    // A 4:3 game drawn in the middle 256 columns, rows 8..215.
    for (int y = 8; y < 216; ++y)
        for (int x = 32; x < 288; ++x) f[static_cast<size_t>(y * w + x)] = 0x204060;
    zen1::ContentTracker t(w, h, 1);
    for (std::uint64_t i = 0; i < 300; ++i) t.feed(f.data(), i);   // 5 s: not decided yet
    CHECK(!t.crop(10.0).has_value());
    for (std::uint64_t i = 300; i < 700; ++i) t.feed(f.data(), i);
    const auto c = t.crop(10.0);
    CHECK(c.has_value());
    CHECK(c->x <= 32 && c->x >= 26);
    CHECK(c->x + c->w >= 288 && c->x + c->w <= 294);
    CHECK(c->y <= 8);
    // A score appears in the border later: the area grows to keep it.
    f[static_cast<size_t>(2 * w + 300)] = 0xffffff;
    t.feed(f.data(), 700);
    CHECK(t.grew());
    const auto c2 = t.crop(10.0);
    CHECK(!c2.has_value() || c2->x + c2->w >= 301);
    // Never more than a quarter off a side.
    zen1::ContentTracker small(w, h, 1);
    std::vector<std::uint32_t> dot(static_cast<size_t>(w * h), 0);
    dot[static_cast<size_t>(112 * w + 160)] = 0xffffff;
    for (std::uint64_t i = 0; i < 700; ++i) small.feed(dot.data(), i);
    const auto c3 = small.crop(10.0);
    CHECK(c3.has_value());
    CHECK(c3->w >= w / 2 && c3->h >= h / 2);
}

TEST("Smart scaling: cropped area keeps its pixel shape, whole multiples when they fill 90 %") {
    zen1::PlanRequest r;
    r.video = {384, 224, 4.0 / 3.0};
    r.screenWidth = 1280;
    r.screenHeight = 1024;
    r.mode = zen1::ScaleMode::Smart;
    auto p = zen1::computeDisplayPlan(r);
    CHECK(p.has_value());
    // 4:3 fit would be 1280x960; 4 x 224 = 896 lines fills 93 % of it: sharp 4x
    CHECK_EQ(p->integerFactor, 4);
    CHECK_EQ(p->game.h, 896);
    CHECK_EQ(p->game.w, 1195);
    CHECK_EQ(p->source.w, 384);
    // Crop the middle 288 columns (4:3 of the 384 is 4:3 * 288/384 = 1.0 wide/tall)
    r.crop = zen1::Rect{48, 0, 288, 224};
    p = zen1::computeDisplayPlan(r);
    CHECK_EQ(p->source.x, 48);
    CHECK(std::abs(static_cast<double>(p->game.w) / p->game.h - 1.0) < 0.01);
    CHECK(p->game.w <= 1280 && p->game.h <= 1024);
    // Square 1024x1024 fit; 4 x 224 = 896 is only 87.5 % of it: best fit, no stretch
    CHECK_EQ(p->game.h, 1024);
    CHECK_EQ(p->game.w, 1024);
    CHECK_EQ(p->integerFactor, 0);
}

TEST("Rect text round trip and rejects junk") {
    const zen1::Rect r{4, 8, 300, 200};
    CHECK((zen1::rectFromString(zen1::rectToString(r)) == std::optional<zen1::Rect>(r)));
    CHECK(!zen1::rectFromString("1,2,0,4").has_value());
    CHECK(!zen1::rectFromString("abc").has_value());
}
