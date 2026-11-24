// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/display_plan.hpp"

#include <algorithm>
#include <cmath>

namespace zen1 {
namespace {

int roundToInt(double v) { return static_cast<int>(std::lround(v)); }

Rect centred(int w, int h, int sw, int sh) {
    return Rect{(sw - w) / 2, (sh - h) / 2, w, h};
}

// Largest w x h with w/h == aspect that fits in sw x sh.
Rect fitAspect(double aspect, int sw, int sh) {
    const double screenAspect = static_cast<double>(sw) / sh;
    int w, h;
    if (screenAspect > aspect) {
        h = sh;
        w = std::min(sw, roundToInt(h * aspect));
    } else {
        w = sw;
        h = std::min(sh, roundToInt(w / aspect));
    }
    return centred(w, h, sw, sh);
}

} // namespace

std::optional<DisplayPlan> computeDisplayPlan(const PlanRequest& req) {
    const auto& v = req.video;
    const int sw = req.screenWidth, sh = req.screenHeight;
    if (v.width <= 0 || v.height <= 0 || v.displayAspect <= 0.0 || sw <= 0 || sh <= 0)
        return std::nullopt;

    const bool sideways = req.rotation == Rotation::R90 || req.rotation == Rotation::R270;
    // Frame as it appears after rotation.
    const int lines = sideways ? v.width : v.height;  // native lines along the vertical
    const int cols = sideways ? v.height : v.width;
    const double aspect = sideways ? 1.0 / v.displayAspect : v.displayAspect;

    DisplayPlan plan;
    plan.rotation = req.rotation;

    switch (req.mode) {
        case ScaleMode::Original:
            plan.game = fitAspect(aspect, sw, sh);
            break;

        case ScaleMode::Stretch:
            plan.game = Rect{0, 0, sw, sh};
            break;

        case ScaleMode::Custom: {
            const double zoom = std::clamp(req.customZoom, 0.1, 1.0);
            const Rect full = fitAspect(aspect, sw, sh);
            plan.game = centred(roundToInt(full.w * zoom), roundToInt(full.h * zoom), sw, sh);
            break;
        }

        case ScaleMode::Integer: {
            // Whole multiple of the native line count; width follows the aspect.
            int n = sh / lines;
            while (n > 0 && roundToInt(n * lines * aspect) > sw) --n;
            if (n == 0) {
                plan.game = fitAspect(aspect, sw, sh);
                plan.fallback = true;
            } else {
                plan.integerFactor = n;
                plan.game = centred(roundToInt(n * lines * aspect), n * lines, sw, sh);
            }
            break;
        }

        case ScaleMode::PixelPerfect: {
            const int n = std::min(sw / cols, sh / lines);
            if (n == 0) {
                plan.game = fitAspect(aspect, sw, sh);
                plan.fallback = true;
            } else {
                plan.integerFactor = n;
                plan.game = centred(n * cols, n * lines, sw, sh);
            }
            break;
        }
    }
    return plan;
}

const char* toString(ScaleMode m) {
    switch (m) {
        case ScaleMode::Original: return "original";
        case ScaleMode::Integer: return "integer";
        case ScaleMode::PixelPerfect: return "pixel-perfect";
        case ScaleMode::Stretch: return "stretch";
        case ScaleMode::Custom: return "custom";
    }
    return "original";
}

std::optional<ScaleMode> scaleModeFromString(const std::string& s) {
    if (s == "original") return ScaleMode::Original;
    if (s == "integer") return ScaleMode::Integer;
    if (s == "pixel-perfect") return ScaleMode::PixelPerfect;
    if (s == "stretch") return ScaleMode::Stretch;
    if (s == "custom") return ScaleMode::Custom;
    return std::nullopt;
}

std::optional<Rotation> rotationFromDegrees(int degrees) {
    switch (((degrees % 360) + 360) % 360) {
        case 0: return Rotation::R0;
        case 90: return Rotation::R90;
        case 180: return Rotation::R180;
        case 270: return Rotation::R270;
        default: return std::nullopt;
    }
}

} // namespace zen1
