// SPDX-License-Identifier: GPL-3.0-or-later
//
// Display plan: where a game's picture goes on the physical screen.
// Pure maths, no graphics API, so it is fully unit-tested.
//
// Rule: the game keeps its intended aspect ratio unless Stretch is chosen.
// The area outside `game` belongs to the surround layer (bezel, artwork,
// colour, or a future expansion source). Nothing may draw over `game`.
#pragma once

#include <optional>
#include <string>

namespace zen1 {

enum class ScaleMode {
    Original,      // largest size keeping the intended aspect ratio (default)
    Integer,       // whole-number multiple of the native line count
    PixelPerfect,  // whole-number multiple in both directions, square pixels
    Stretch,       // fill the screen (only when the user chooses it)
    Custom,        // Original multiplied by a user zoom
};

enum class Rotation { R0 = 0, R90 = 90, R180 = 180, R270 = 270 };

struct GameVideo {
    int width = 0;               // native frame width in pixels (e.g. 384)
    int height = 0;              // native frame height (e.g. 224)
    double displayAspect = 4.0 / 3.0;  // shape the frame was shown at, unrotated
};

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const Rect&) const = default;
};

struct DisplayPlan {
    Rect game;            // destination of the (rotated) game image
    Rotation rotation = Rotation::R0;
    int integerFactor = 0;  // >0 when Integer/PixelPerfect applied
    bool fallback = false;  // requested mode impossible, Original used instead
};

struct PlanRequest {
    GameVideo video;
    int screenWidth = 0;
    int screenHeight = 0;
    ScaleMode mode = ScaleMode::Original;
    Rotation rotation = Rotation::R0;
    double customZoom = 1.0;  // used by Custom, clamped to (0.1, 1.0]
};

std::optional<DisplayPlan> computeDisplayPlan(const PlanRequest& req);

const char* toString(ScaleMode m);
std::optional<ScaleMode> scaleModeFromString(const std::string& s);
std::optional<Rotation> rotationFromDegrees(int degrees);

} // namespace zen1
