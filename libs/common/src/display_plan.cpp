// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/display_plan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
    const int sw = req.screenWidth, sh = req.screenHeight;
    if (req.video.width <= 0 || req.video.height <= 0 || req.video.displayAspect <= 0.0 || sw <= 0 || sh <= 0)
        return std::nullopt;
    // A crop keeps the pixel shape: the shown part's aspect follows from it.
    GameVideo v = req.video;
    Rect source{0, 0, v.width, v.height};
    if (req.mode == ScaleMode::Smart && req.crop && req.crop->w > 0 && req.crop->h > 0 &&
        req.crop->x >= 0 && req.crop->y >= 0 && req.crop->x + req.crop->w <= v.width && req.crop->y + req.crop->h <= v.height) {
        source = *req.crop;
        v.displayAspect = req.video.displayAspect * (static_cast<double>(source.w) / v.width) / (static_cast<double>(source.h) / v.height);
        v.width = source.w;
        v.height = source.h;
    }

    const bool sideways = req.rotation == Rotation::R90 || req.rotation == Rotation::R270;
    // Frame as it appears after rotation.
    const int lines = sideways ? v.width : v.height;  // native lines along the vertical
    const int cols = sideways ? v.height : v.width;
    const double aspect = sideways ? 1.0 / v.displayAspect : v.displayAspect;

    DisplayPlan plan;
    plan.rotation = req.rotation;
    plan.source = source;

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

        case ScaleMode::Smart: {
            // Whole multiple of the line count when it fills at least 90 %
            // of the best fit (sharp, even scanlines); otherwise the best fit.
            const Rect fit = fitAspect(aspect, sw, sh);
            int n = sh / lines;
            while (n > 0 && roundToInt(n * lines * aspect) > sw) --n;
            if (n > 0 && n * lines >= fit.h * 0.9) {
                plan.integerFactor = n;
                plan.game = centred(roundToInt(n * lines * aspect), n * lines, sw, sh);
            } else {
                plan.game = fit;
            }
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
    // Placement: only moves the picture inside the screen, never resizes it.
    switch (req.anchor) {
        case Anchor::Top: plan.game.y = 0; break;
        case Anchor::Bottom: plan.game.y = sh - plan.game.h; break;
        case Anchor::Left: plan.game.x = 0; break;
        case Anchor::Right: plan.game.x = sw - plan.game.w; break;
        case Anchor::Centre: break;
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
        case ScaleMode::Smart: return "smart";
    }
    return "original";
}

std::optional<ScaleMode> scaleModeFromString(const std::string& s) {
    if (s == "original") return ScaleMode::Original;
    if (s == "integer") return ScaleMode::Integer;
    if (s == "pixel-perfect") return ScaleMode::PixelPerfect;
    if (s == "stretch") return ScaleMode::Stretch;
    if (s == "custom") return ScaleMode::Custom;
    if (s == "smart") return ScaleMode::Smart;
    return std::nullopt;
}

ContentTracker::ContentTracker(int width, int height, int every)
    : w_(width), h_(height), every_(std::max(1, every)), x0_(width), y0_(height), x1_(-1), y1_(-1) {}

void ContentTracker::feed(const std::uint32_t* px, std::uint64_t frame) {
    if (!px || frame % static_cast<std::uint64_t>(every_) != 0) return;
    ++samples_;
    const int ox0 = x0_, oy0 = y0_, ox1 = x1_, oy1 = y1_;
    for (int y = 0; y < h_; y += 2) {
        const std::uint32_t* row = px + static_cast<size_t>(y) * static_cast<size_t>(w_);
        for (int x = 0; x < w_; x += 2) {
            const std::uint32_t c = row[x];
            const int luma = static_cast<int>(((c >> 16) & 0xff) * 3 + ((c >> 8) & 0xff) * 6 + (c & 0xff)) / 10;
            if (luma < 20) continue;  // black (and near-black monitor bias)
            x0_ = std::min(x0_, x); x1_ = std::max(x1_, x + 1);
            y0_ = std::min(y0_, y); y1_ = std::max(y1_, y + 1);
        }
    }
    if (x1_ >= 0 && (x0_ < ox0 || y0_ < oy0 || x1_ > ox1 || y1_ > oy1) && (seeded_ || ox1 >= 0)) grew_ = true;
}

void ContentTracker::seed(const Rect& r) {
    x0_ = std::clamp(r.x, 0, w_ - 1);
    y0_ = std::clamp(r.y, 0, h_ - 1);
    x1_ = std::clamp(r.x + r.w, x0_ + 1, w_);
    y1_ = std::clamp(r.y + r.h, y0_ + 1, h_);
    seeded_ = true;
    samples_ = 1'000'000;  // a saved profile is trusted straight away
}

Rect ContentTracker::lit() const {
    if (x1_ < 0) return Rect{0, 0, 0, 0};
    return Rect{x0_, y0_, x1_ - x0_, y1_ - y0_};
}

std::optional<Rect> ContentTracker::crop(double minSeconds) const {
    if (x1_ < 0) return std::nullopt;
    if (static_cast<double>(samples_) * every_ / 60.0 < minSeconds) return std::nullopt;
    // A small safety margin, even-aligned, and never more than a quarter
    // of the picture off any side.
    int x0 = std::max(0, (x0_ - 2) & ~1), y0 = std::max(0, (y0_ - 2) & ~1);
    int x1 = std::min(w_, (x1_ + 3) & ~1), y1 = std::min(h_, (y1_ + 3) & ~1);
    x0 = std::min(x0, w_ / 4); y0 = std::min(y0, h_ / 4);
    x1 = std::max(x1, w_ - w_ / 4); y1 = std::max(y1, h_ - h_ / 4);
    const Rect r{x0, y0, x1 - x0, y1 - y0};
    const bool worth = r.x > w_ * 3 / 100 || r.y > h_ * 3 / 100 || w_ - (r.x + r.w) > w_ * 3 / 100 ||
                       h_ - (r.y + r.h) > h_ * 3 / 100;
    if (!worth) return std::nullopt;
    return r;
}

std::string rectToString(const Rect& r) {
    return std::to_string(r.x) + "," + std::to_string(r.y) + "," + std::to_string(r.w) + "," + std::to_string(r.h);
}

std::optional<Rect> rectFromString(const std::string& s) {
    Rect r;
    if (std::sscanf(s.c_str(), "%d,%d,%d,%d", &r.x, &r.y, &r.w, &r.h) != 4 || r.w <= 0 || r.h <= 0 || r.x < 0 || r.y < 0)
        return std::nullopt;
    return r;
}

std::optional<Anchor> anchorFromString(const std::string& s) {
    if (s == "centre" || s == "center") return Anchor::Centre;
    if (s == "top") return Anchor::Top;
    if (s == "bottom") return Anchor::Bottom;
    if (s == "left") return Anchor::Left;
    if (s == "right") return Anchor::Right;
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
