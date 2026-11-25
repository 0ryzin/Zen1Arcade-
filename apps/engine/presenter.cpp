// SPDX-License-Identifier: GPL-3.0-or-later
#include "presenter.hpp"

#include "zen1/log.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace fs = std::filesystem;

namespace zen1::engine {

// "VOLUME 50%" panel with a speaker icon and three level bars, big enough to
// read from across the room.
void drawVolume(SDL_Renderer* r, int w, int h, int volume) {
    const float u = static_cast<float>(std::min(w, h)) / 240.0f;  // 1 unit = 1/240 of the short side
    const float bw = 150 * u, bh = 70 * u;
    const float x = (static_cast<float>(w) - bw) / 2, y = (static_cast<float>(h) - bh) / 2;
    SDL_SetRenderDrawColor(r, 6, 4, 22, 235);
    const SDL_FRect box{x, y, bw, bh};
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 255, 60, 200, 255);
    for (float t = 0; t < 2 * u; t += 1) {
        const SDL_FRect o{x + t, y + t, bw - 2 * t, bh - 2 * t};
        SDL_RenderRect(r, &o);
    }
    // Speaker
    SDL_SetRenderDrawColor(r, 40, 230, 255, 255);
    const float sx = x + 14 * u, sy = y + 16 * u;
    const SDL_FRect body{sx, sy + 8 * u, 8 * u, 12 * u};
    SDL_RenderFillRect(r, &body);
    for (int i = 0; i < 10; ++i) {
        const SDL_FRect cone{sx + 8 * u + static_cast<float>(i) * u, sy + 8 * u - static_cast<float>(i) * 0.8f * u,
                             u, 12 * u + static_cast<float>(i) * 1.6f * u};
        SDL_RenderFillRect(r, &cone);
    }
    if (volume == 0) {  // muted: a red X
        SDL_SetRenderDrawColor(r, 255, 70, 70, 255);
        for (int i = 0; i < 12; ++i) {
            const float t = static_cast<float>(i) * u;
            const SDL_FRect a{sx + 22 * u + t, sy + 8 * u + t, 2 * u, 2 * u};
            const SDL_FRect b{sx + 22 * u + t, sy + 20 * u - t, 2 * u, 2 * u};
            SDL_RenderFillRect(r, &a);
            SDL_RenderFillRect(r, &b);
        }
    }
    // Three level bars: 0%, 50%, 100%
    const int lit = volume >= 100 ? 3 : volume >= 50 ? 2 : volume > 0 ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        const float bx = x + 60 * u + static_cast<float>(i) * 26 * u, bhh = (10 + 9 * static_cast<float>(i)) * u;
        const SDL_FRect bar{bx, y + 40 * u - bhh, 18 * u, bhh};
        // bar 1 = low (lit at 50 and 100), bar 2 = mid, bar 3 = full
        const bool on = (i == 0 && volume > 0) || (i == 1 && volume >= 50) || (i == 2 && volume >= 100);
        (void)lit;
        if (on) SDL_SetRenderDrawColor(r, i == 2 ? 255 : 40, i == 2 ? 60 : 230, i == 2 ? 200 : 255, 255);
        else SDL_SetRenderDrawColor(r, 50, 46, 70, 255);
        SDL_RenderFillRect(r, &bar);
    }
    // Text
    const std::string text = "VOLUME " + std::to_string(volume) + "%";
    const float ts = std::max(1.0f, std::floor(2.6f * u));
    SDL_SetRenderScale(r, ts, ts);
    SDL_SetRenderDrawColor(r, 240, 240, 255, 255);
    const float tw = 8.0f * static_cast<float>(text.size());
    SDL_RenderDebugText(r, (static_cast<float>(w) / ts - tw) / 2, (y + 50 * u) / ts, text.c_str());
    SDL_SetRenderScale(r, 1, 1);
}


Presenter::Presenter(SDL_Renderer* r, const fs::path& artworkDir, const std::string& set, const std::string& bezel)
    : r_(r), kind_(bezel) {
    if (kind_ == "scenery") {
        // A full-screen background made for this game (drawn by the user,
        // or generated offline and cached): Artwork/Scenery/<set>.png.
        for (const fs::path& p : {artworkDir / "Scenery" / (set + ".png"), artworkDir / "Scenery" / (set + ".bmp")}) {
            std::error_code ec;
            if (!fs::exists(p, ec)) continue;
            SDL_Surface* s = p.extension() == ".png" ? SDL_LoadPNG(p.string().c_str()) : SDL_LoadBMP(p.string().c_str());
            if (!s) continue;
            artwork_ = SDL_CreateTextureFromSurface(r_, s);
            SDL_SetTextureScaleMode(artwork_, SDL_SCALEMODE_LINEAR);
            artworkW_ = s->w;
            artworkH_ = s->h;
            SDL_DestroySurface(s);
            log::info("scenery: " + p.string());
            break;
        }
        if (!artwork_) kind_ = "black";
    }
    if (kind_ == "artwork") {
        for (const fs::path& p : {artworkDir / "Bezels" / (set + ".bmp"), artworkDir / "Bezels" / "default.bmp"}) {
            std::error_code ec;
            if (!fs::exists(p, ec)) continue;
            if (SDL_Surface* s = SDL_LoadBMP(p.string().c_str())) {
                artwork_ = SDL_CreateTextureFromSurface(r_, s);
                SDL_DestroySurface(s);
                log::info("bezel artwork: " + p.string());
                break;
            }
        }
        if (!artwork_) kind_ = "black";
    }
}

Presenter::~Presenter() {
    if (artwork_) SDL_DestroyTexture(artwork_);
    if (backdrop_) SDL_DestroyTexture(backdrop_);
    if (blur_) SDL_DestroyTexture(blur_);
}

// Zen1 neon backdrop: deep violet sky, a glowing horizon and a perspective
// grid floor. Generated once per screen size.
void Presenter::ensureBackdrop(int w, int h) {
    if (backdrop_ && bw_ == w && bh_ == h) return;
    if (backdrop_) SDL_DestroyTexture(backdrop_);
    bw_ = w;
    bh_ = h;
    std::vector<std::uint32_t> px(static_cast<size_t>(w) * static_cast<size_t>(h));
    const double horizon = h * 0.60, cx = w / 2.0;
    for (int y = 0; y < h; ++y) {
        const double t = static_cast<double>(y) / h;
        for (int x = 0; x < w; ++x) {
            double r = 6 + 14 * t, g = 4 + 4 * t, b = 22 + 24 * t;
            const double dh = (y - horizon) / h;
            const double glow = std::exp(-dh * dh * 900.0);  // horizon glow
            r += 120 * glow;
            g += 20 * glow;
            b += 140 * glow;
            if (y > horizon) {
                const double depth = (y - horizon) / (h - horizon);  // 0 at horizon, 1 at bottom
                const double z = 1.0 / std::max(depth, 0.02);
                const double rowPhase = std::fmod(z * 2.0, 1.0);
                const double xs = (x - cx) / (depth * w + 1e-6) * 8.0;
                const double colPhase = std::fabs(xs - std::round(xs));
                const double lineW = 0.02 + 0.03 * depth;
                double line = 0.0;
                if (rowPhase < lineW * 2) line = 1.0;
                if (colPhase < lineW) line = 1.0;
                const double fade = std::min(1.0, depth * 3.0);
                r += 200 * line * fade;
                g += 30 * line * fade;
                b += 170 * line * fade;
            }
            if ((y & 3) == 0) { r *= 0.85; g *= 0.85; b *= 0.85; }  // faint scanlines
            const auto c = [](double v) { return static_cast<std::uint32_t>(std::clamp(v, 0.0, 255.0)); };
            px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                c(r) << 16 | c(g) << 8 | c(b);
        }
    }
    backdrop_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    SDL_UpdateTexture(backdrop_, nullptr, px.data(), w * 4);
}

void Presenter::draw(SDL_Texture* game, const DisplayPlan* plan, Rotation rotation, const Overlay& ov) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(r_, &w, &h);
    SDL_SetRenderDrawColor(r_, 0, 0, 0, 255);
    SDL_RenderClear(r_);
    if (kind_ == "scenery" && artwork_) {
        // Cover the screen keeping the image's shape (crop its edges, never stretch).
        const float sa = static_cast<float>(artworkW_) / static_cast<float>(std::max(1, artworkH_));
        const float da = static_cast<float>(w) / static_cast<float>(std::max(1, h));
        SDL_FRect src{0, 0, static_cast<float>(artworkW_), static_cast<float>(artworkH_)};
        if (sa > da) { src.w = static_cast<float>(artworkH_) * da; src.x = (static_cast<float>(artworkW_) - src.w) / 2; }
        else { src.h = static_cast<float>(artworkW_) / da; src.y = (static_cast<float>(artworkH_) - src.h) / 2; }
        SDL_RenderTexture(r_, artwork_, &src, nullptr);
    } else if (kind_ == "artwork" && artwork_) {
        SDL_RenderTexture(r_, artwork_, nullptr, nullptr);
    } else if (kind_ == "neon") {
        ensureBackdrop(w, h);
        SDL_RenderTexture(r_, backdrop_, nullptr, nullptr);
    } else if (kind_ == "blur" && plan) {
        // The game itself, shrunk to a few dozen pixels and smoothed back
        // up: a soft, moving glow of its colours fills the unused screen.
        const SDL_FRect src{static_cast<float>(plan->source.x), static_cast<float>(plan->source.y),
                            static_cast<float>(plan->source.w), static_cast<float>(plan->source.h)};
        if (!blur_) {
            blur_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, 24, 18);
            if (blur_) SDL_SetTextureScaleMode(blur_, SDL_SCALEMODE_LINEAR);
        }
        if (blur_) {
            SDL_Texture* prev = SDL_GetRenderTarget(r_);
            SDL_SetRenderTarget(r_, blur_);
            SDL_ScaleMode mode = SDL_SCALEMODE_NEAREST;
            SDL_GetTextureScaleMode(game, &mode);
            SDL_SetTextureScaleMode(game, SDL_SCALEMODE_LINEAR);
            SDL_RenderTexture(r_, game, &src, nullptr);
            SDL_SetTextureScaleMode(game, mode);
            SDL_SetRenderTarget(r_, prev);
            SDL_RenderTextureRotated(r_, blur_, nullptr, nullptr, static_cast<double>(rotation), nullptr, SDL_FLIP_NONE);
            SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r_, 0, 0, 0, 150);  // darkened, so the game stays the focus
            SDL_RenderFillRect(r_, nullptr);
        }
    }

    if (plan) {
        const auto& g = plan->game;
        if (kind_ == "neon") {  // soft cyan frame glow just outside the picture
            SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
            for (int i = 1; i <= 8; ++i) {
                SDL_SetRenderDrawColor(r_, 40, 230, 255, static_cast<Uint8>(150 / i));
                const SDL_FRect fr{static_cast<float>(g.x - i), static_cast<float>(g.y - i),
                                   static_cast<float>(g.w + 2 * i), static_cast<float>(g.h + 2 * i)};
                SDL_RenderRect(r_, &fr);
            }
            SDL_SetRenderDrawColor(r_, 0, 0, 0, 255);
            const SDL_FRect bg{static_cast<float>(g.x), static_cast<float>(g.y), static_cast<float>(g.w),
                               static_cast<float>(g.h)};
            SDL_RenderFillRect(r_, &bg);
        }
        const bool sideways = rotation == Rotation::R90 || rotation == Rotation::R270;
        // SDL rotates around the rectangle centre: give it the unrotated size.
        const float rw = static_cast<float>(sideways ? g.h : g.w);
        const float rh = static_cast<float>(sideways ? g.w : g.h);
        const float cx = static_cast<float>(g.x) + static_cast<float>(g.w) / 2.0f;
        const float cy = static_cast<float>(g.y) + static_cast<float>(g.h) / 2.0f;
        const SDL_FRect dst{cx - rw / 2.0f, cy - rh / 2.0f, rw, rh};
        const SDL_FRect src{static_cast<float>(plan->source.x), static_cast<float>(plan->source.y),
                            static_cast<float>(plan->source.w), static_cast<float>(plan->source.h)};
        SDL_RenderTextureRotated(r_, game, plan->source.w > 0 ? &src : nullptr, &dst, static_cast<double>(rotation), nullptr,
                                 SDL_FLIP_NONE);
    }

    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    const float scale = h >= 900 ? 2.0f : 1.0f;
    if (!ov.lines.empty()) {
        size_t longest = 0;
        for (const auto& l : ov.lines) longest = std::max(longest, l.size());
        const float lh = 12.0f;
        SDL_SetRenderDrawColor(r_, 5, 4, 20, 200);
        const SDL_FRect box{8, 8, (16.0f + static_cast<float>(longest) * 8.0f) * scale,
                            (12.0f + lh * static_cast<float>(ov.lines.size())) * scale};
        SDL_RenderFillRect(r_, &box);
        SDL_SetRenderDrawColor(r_, 40, 230, 255, 255);
        SDL_RenderRect(r_, &box);
        SDL_SetRenderScale(r_, scale, scale);
        for (size_t i = 0; i < ov.lines.size(); ++i) {
            SDL_SetRenderDrawColor(r_, i == ov.lines.size() - 1 ? 255 : 230, i == ov.lines.size() - 1 ? 80 : 240,
                                   i == ov.lines.size() - 1 ? 220 : 255, 255);
            SDL_RenderDebugText(r_, 12.0f / scale + 8, 12.0f / scale + 6 + lh * static_cast<float>(i),
                                ov.lines[i].c_str());
        }
        SDL_SetRenderScale(r_, 1, 1);
    }
    if (ov.holdProgress > 0.02) {
        const float bw = static_cast<float>(w) * 0.5f, bh = 18.0f * scale;
        const float bx = (static_cast<float>(w) - bw) / 2, by = static_cast<float>(h) - bh - 40 * scale;
        SDL_SetRenderDrawColor(r_, 5, 4, 20, 220);
        const SDL_FRect back{bx - 6, by - 26 * scale, bw + 12, bh + 32 * scale};
        SDL_RenderFillRect(r_, &back);
        SDL_SetRenderDrawColor(r_, 255, 60, 200, 255);
        const SDL_FRect fill{bx, by, bw * static_cast<float>(std::min(1.0, ov.holdProgress)), bh};
        SDL_RenderFillRect(r_, &fill);
        SDL_SetRenderDrawColor(r_, 40, 230, 255, 255);
        const SDL_FRect outline{bx, by, bw, bh};
        SDL_RenderRect(r_, &outline);
        SDL_SetRenderScale(r_, scale, scale);
        const float tw = 8.0f * static_cast<float>(ov.holdLabel.size());
        SDL_RenderDebugText(r_, (static_cast<float>(w) / scale - tw) / 2, (by - 20 * scale) / scale, ov.holdLabel.c_str());
        SDL_SetRenderScale(r_, 1, 1);
    }
    if (ov.volume >= 0) drawVolume(r_, w, h, ov.volume);
}

}  // namespace zen1::engine
