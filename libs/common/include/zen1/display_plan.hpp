// SPDX-License-Identifier: GPL-3.0-or-later
//
// Display plan: where a game's picture goes on the physical screen.
// Pure maths, no graphics API, so it is fully unit-tested.
//
// Rule: the game keeps its intended aspect ratio unless Stretch is chosen.
// The area outside `game` belongs to the surround layer (bezel, artwork,
// colour, or a future expansion source). Nothing may draw over `game`.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace zen1 {

enum class ScaleMode {
    Original,      // largest size keeping the intended aspect ratio (default)
    Integer,       // whole-number multiple of the native line count
    PixelPerfect,  // whole-number multiple in both directions, square pixels
    Stretch,       // fill the screen (only when the user chooses it)
    Custom,        // Original multiplied by a user zoom
    Smart,         // the game's real picture area (see ContentTracker), whole
                   // multiples when they fill at least 90 %, never stretched
};

enum class Rotation { R0 = 0, R90 = 90, R180 = 180, R270 = 270 };

// Where the picture sits when it does not fill the screen. The free space
// goes to the other side(s): Bottom leaves room above for scenery, etc.
enum class Anchor { Centre, Top, Bottom, Left, Right };

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
    Rect source;          // part of the native frame shown (unrotated); whole frame unless cropped
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
    std::optional<Rect> crop; // Smart: native-pixel area that holds the picture
    Anchor anchor = Anchor::Centre;
};

// Finds the part of a game's frame that is ever used, by watching frames:
// rows and columns that have stayed black since the game started are
// border, everything that has ever lit up (scores, lives, credits) is kept.
// The area only ever grows, so a HUD that appears later is never cut.
// Cheap: one in `every` frames, every second pixel.
class ContentTracker {
public:
    ContentTracker(int width, int height, int every = 15);
    void feed(const std::uint32_t* xrgb, std::uint64_t frameNumber);
    // A crop once enough was seen (seconds of frames at 60 Hz) and the
    // borders are worth removing (more than 3 % of a side); otherwise none.
    std::optional<Rect> crop(double minSeconds = 10.0) const;
    void seed(const Rect& r);  // start from a saved profile
    bool grew() { const bool g = grew_; grew_ = false; return g; }
    Rect lit() const;

private:
    int w_, h_, every_;
    int x0_, y0_, x1_, y1_;  // lit box, end exclusive; x1 < 0 = nothing yet
    std::uint64_t samples_ = 0;
    bool grew_ = false, seeded_ = false;
};

std::string rectToString(const Rect& r);
std::optional<Rect> rectFromString(const std::string& s);

std::optional<DisplayPlan> computeDisplayPlan(const PlanRequest& req);

const char* toString(ScaleMode m);
std::optional<ScaleMode> scaleModeFromString(const std::string& s);
std::optional<Rotation> rotationFromDegrees(int degrees);
std::optional<Anchor> anchorFromString(const std::string& s);  // centre|center|top|bottom|left|right

} // namespace zen1
