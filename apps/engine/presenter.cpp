// SPDX-License-Identifier: GPL-3.0-or-later
#include "presenter.hpp"

#include "zen1/log.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace fs = std::filesystem;

namespace zen1::engine {

Presenter::Presenter(SDL_Renderer* r, const fs::path& artworkDir, const std::string& set, const std::string& bezel)
    : r_(r), kind_(bezel) {
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
        if (!artwork_) kind_ = "neon";
    }
}

Presenter::~Presenter() {
    if (artwork_) SDL_DestroyTexture(artwork_);
    if (backdrop_) SDL_DestroyTexture(backdrop_);
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
    if (kind_ == "artwork" && artwork_) {
        SDL_RenderTexture(r_, artwork_, nullptr, nullptr);
    } else if (kind_ == "neon") {
        ensureBackdrop(w, h);
        SDL_RenderTexture(r_, backdrop_, nullptr, nullptr);
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
        SDL_RenderTextureRotated(r_, game, nullptr, &dst, static_cast<double>(rotation), nullptr, SDL_FLIP_NONE);
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
}

}  // namespace zen1::engine
