// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/display_plan.hpp"
#include "zen1_test.hpp"

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
