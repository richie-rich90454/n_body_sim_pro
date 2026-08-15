#include "application/AppIcon.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace n_body_sim_pro::application {

namespace {

constexpr int kSize = 256;
constexpr float kHalf = 128.0f;

struct Rgba {
    float r;
    float g;
    float b;
    float a;
};

// Palette mirrors docs-site/public/favicon.svg.
constexpr Rgba kBackground{0.043f, 0.043f, 0.063f, 1.0f};   // #0b0b10
constexpr Rgba kBorder{0.137f, 0.149f, 0.212f, 1.0f};       // #232636
constexpr Rgba kOrbit{0.290f, 0.463f, 0.502f, 1.0f};        // #4a7680
constexpr Rgba kBody{0.498f, 0.722f, 0.776f, 1.0f};         // #7fb8c6
constexpr Rgba kSatellite{0.847f, 0.706f, 0.416f, 1.0f};    // #d8b46a
constexpr Rgba kTrail{0.847f, 0.706f, 0.416f, 0.45f};       // #d8b46a @ 45%

bool inside_rounded_rect(float x, float y, float left, float top, float right,
                         float bottom, float radius) {
    const float cx = std::clamp(x, left + radius, right - radius);
    const float cy = std::clamp(y, top + radius, bottom - radius);
    const float dx = x - cx;
    const float dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

bool inside_orbit_ring(float x, float y) {
    const float dx = x - kHalf;
    const float dy = y - kHalf;
    constexpr float kCos25 = 0.906307787f;
    constexpr float kSin25 = 0.422618262f;
    const float lx = dx * kCos25 - dy * kSin25;
    const float ly = dx * kSin25 + dy * kCos25;
    constexpr float kRx = 92.0f;
    constexpr float kRy = 42.0f;
    const float rho = std::sqrtf((lx / kRx) * (lx / kRx) + (ly / kRy) * (ly / kRy));
    return std::fabsf(rho - 1.0f) * 46.0f <= 3.0f;
}

bool inside_circle(float x, float y, float cx, float cy, float radius) {
    const float dx = x - cx;
    const float dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

Rgba composite(const Rgba& dst, const Rgba& src) {
    const float out_alpha = src.a + dst.a * (1.0f - src.a);
    if (out_alpha <= 0.0f) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }
    return {
        (src.r * src.a + dst.r * dst.a * (1.0f - src.a)) / out_alpha,
        (src.g * src.a + dst.g * dst.a * (1.0f - src.a)) / out_alpha,
        (src.b * src.a + dst.b * dst.a * (1.0f - src.a)) / out_alpha,
        out_alpha,
    };
}

Rgba sample(float x, float y) {
    if (!inside_rounded_rect(x, y, 0.0f, 0.0f, 256.0f, 256.0f, 52.0f)) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }
    Rgba color = kBackground;
    if (!inside_rounded_rect(x, y, 4.0f, 4.0f, 252.0f, 252.0f, 48.0f)) {
        return composite(color, kBorder);
    }
    if (inside_orbit_ring(x, y)) {
        return composite(color, kOrbit);
    }
    if (inside_circle(x, y, kHalf, kHalf, 22.0f)) {
        return composite(color, kBody);
    }
    if (inside_circle(x, y, 208.0f, 116.0f, 12.0f)) {
        return composite(color, kSatellite);
    }
    if (inside_circle(x, y, 180.0f, 88.0f, 6.4f)) {
        return composite(color, kTrail);
    }
    return color;
}

SDL_Surface* create_icon_surface() {
    SDL_Surface* surface = SDL_CreateSurface(kSize, kSize, SDL_PIXELFORMAT_RGBA8888);
    if (surface == nullptr) {
        return nullptr;
    }
    if (!SDL_LockSurface(surface)) {
        SDL_DestroySurface(surface);
        return nullptr;
    }

    constexpr int kSamples = 4;
    for (int y = 0; y < kSize; ++y) {
        uint8_t* row = static_cast<uint8_t*>(surface->pixels) + y * surface->pitch;
        for (int x = 0; x < kSize; ++x) {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            float a = 0.0f;
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    const float px = static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) /
                                                                static_cast<float>(kSamples);
                    const float py = static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) /
                                                                static_cast<float>(kSamples);
                    const Rgba c = sample(px, py);
                    r += c.r;
                    g += c.g;
                    b += c.b;
                    a += c.a;
                }
            }
            const float scale = 1.0f / static_cast<float>(kSamples * kSamples);
            row[x * 4 + 0] = static_cast<uint8_t>(std::clamp(r * scale, 0.0f, 1.0f) * 255.0f + 0.5f);
            row[x * 4 + 1] = static_cast<uint8_t>(std::clamp(g * scale, 0.0f, 1.0f) * 255.0f + 0.5f);
            row[x * 4 + 2] = static_cast<uint8_t>(std::clamp(b * scale, 0.0f, 1.0f) * 255.0f + 0.5f);
            row[x * 4 + 3] = static_cast<uint8_t>(std::clamp(a * scale, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }

    SDL_UnlockSurface(surface);
    return surface;
}

}  // namespace

void apply_window_icon(SDL_Window* window) {
    SDL_Surface* icon = create_icon_surface();
    if (icon == nullptr) {
        std::fprintf(stderr, "N-Body Sim Pro: failed to create window icon surface: %s\n",
                     SDL_GetError());
        return;
    }
    if (!SDL_SetWindowIcon(window, icon)) {
        std::fprintf(stderr, "N-Body Sim Pro: failed to set window icon: %s\n", SDL_GetError());
    }
    SDL_DestroySurface(icon);
}

}  // namespace n_body_sim_pro::application
