// SPDX-License-Identifier: GPL-3.0-or-later
//
// Puts the game picture on the screen: the surround (Zen1 neon backdrop,
// the user's own bezel artwork, or black) first, the game image on top at
// the display plan's rectangle, then small overlays (control hints, exit
// progress). The game image itself is never altered by the surround.
#pragma once

#include "zen1/display_plan.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <vector>

namespace zen1::engine {

struct Overlay {
    std::vector<std::string> lines;  // shown top-left for a few seconds
    double holdProgress = 0.0;       // 0..1 while a player button is held
    std::string holdLabel;
    int volume = -1;                 // 0 / 50 / 100 while the volume overlay shows
};

class Presenter {
public:
    // bezel: "neon" (default), "black", "scenery" (Artwork/Scenery/<set>.png,
    // a full-screen background for that game), "blur" (the game's own colours,
    // blurred and darkened) or "artwork" (Artwork/Bezels/<set>.bmp, then
    // Artwork/Bezels/default.bmp; falls back to neon when absent).
    Presenter(SDL_Renderer* r, const std::filesystem::path& artworkDir, const std::string& set,
              const std::string& bezel);
    ~Presenter();
    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;

    void draw(SDL_Texture* game, const DisplayPlan* plan, Rotation rotation, const Overlay& ov);

private:
    void ensureBackdrop(int w, int h);
    SDL_Renderer* r_;
    std::string kind_;
    SDL_Texture* artwork_ = nullptr;
    SDL_Texture* backdrop_ = nullptr;
    SDL_Texture* blur_ = nullptr;
    int artworkW_ = 0, artworkH_ = 0;
    int bw_ = 0, bh_ = 0;
};

}  // namespace zen1::engine
