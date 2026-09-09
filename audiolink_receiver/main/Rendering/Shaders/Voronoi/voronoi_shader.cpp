#include "voronoi_shader.h"

#include <cmath>
#include <cstdint>
#include "shader_globals.h"
#include "color_utils.h"

namespace {

// Q16.16 fixed point: all cell/hash/distance math below stays in this integer format;
// the only float conversions are the UV input and the final Color/hsv_to_rgb output.
constexpr int32_t FP_SHIFT = 16;
constexpr int32_t FP_ONE = 1 << FP_SHIFT;

constexpr int32_t GRID_SIZE = 8;              // feature cells across the UV space
constexpr int32_t CELL_SIZE = FP_ONE / GRID_SIZE;
constexpr int32_t WOBBLE_AMPLITUDE = CELL_SIZE / 4;
constexpr uint32_t ANIM_PERIOD_MS = 6000;     // full point-wobble cycle
constexpr uint32_t GLOW_LUT_SHIFT = 16;

// Classic multiplicative integer hash; no floating point involved.
uint32_t hash2(int32_t x, int32_t y, uint32_t salt) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u
               + static_cast<uint32_t>(y) * 668265263u
               + salt * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return h;
}

// Triangle wave of period FP_ONE, oscillating in [-amplitude, amplitude].
int32_t triangle_wave(uint32_t phase, int32_t amplitude) {
    uint32_t p = phase & static_cast<uint32_t>(FP_ONE - 1);
    int32_t tri = (p < static_cast<uint32_t>(FP_ONE / 2))
                      ? static_cast<int32_t>(p)
                      : static_cast<int32_t>(FP_ONE - p);
    return (tri * 4 * amplitude) / FP_ONE - amplitude;
}

// Animated position of the feature point that "owns" fixed-point cell (cx, cy).
void feature_point(int32_t cx, int32_t cy, uint32_t t, int32_t *out_x, int32_t *out_y) {
    const uint32_t hx = hash2(cx, cy, 1);
    const uint32_t hy = hash2(cx, cy, 2);

    const int32_t jitter_x = static_cast<int32_t>(hx % static_cast<uint32_t>(CELL_SIZE));
    const int32_t jitter_y = static_cast<int32_t>(hy % static_cast<uint32_t>(CELL_SIZE));

    const int32_t wobble_x = triangle_wave(t + hx, WOBBLE_AMPLITUDE);
    const int32_t wobble_y = triangle_wave(t + hy, WOBBLE_AMPLITUDE);

    *out_x = cx * CELL_SIZE + jitter_x + wobble_x;
    *out_y = cy * CELL_SIZE + jitter_y + wobble_y;
}

int32_t clamp_fp(int32_t value, int32_t lo, int32_t hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

} // namespace

void VoronoiShader::initialize_glow_lut() {
    if (glow_lut_initialized_) {
        return;
    }

    for (uint32_t index = 0; index < GLOW_LUT_SIZE; ++index) {
        const uint32_t distance_sq = index << GLOW_LUT_SHIFT;
        const int32_t distance = static_cast<int32_t>(sqrtf(static_cast<float>(distance_sq)));
        const int32_t glow = FP_ONE - clamp_fp(
            (distance * FP_ONE) / (CELL_SIZE * 3 / 2), 0, FP_ONE / 2);
        glow_lut_[index] = static_cast<uint16_t>(glow >= FP_ONE ? FP_ONE - 1 : glow);
    }
    glow_lut_initialized_ = true;
}

void VoronoiShader::update_feature_cache(uint32_t t) {
    for (int32_t cache_y = 0; cache_y < FEATURE_CACHE_SIZE; ++cache_y) {
        for (int32_t cache_x = 0; cache_x < FEATURE_CACHE_SIZE; ++cache_x) {
            feature_point(cache_x - 1, cache_y - 1, t,
                          &feature_x_[cache_y][cache_x],
                          &feature_y_[cache_y][cache_x]);

            const uint32_t color_hash = hash2(cache_x - 1, cache_y - 1, 3);
            const float hue = static_cast<float>(color_hash % 360u);
            hsv_to_rgb(hue, 1.0f, 1.0f,
                       &color_r_[cache_y][cache_x],
                       &color_g_[cache_y][cache_x],
                       &color_b_[cache_y][cache_x]);
        }
    }
}

void VoronoiShader::begin_frame() {
    initialize_glow_lut();
    const uint32_t now_ms = static_cast<uint32_t>(_Time * 1000.0f);
    frame_time_ = (static_cast<uint64_t>(now_ms % ANIM_PERIOD_MS) * FP_ONE) / ANIM_PERIOD_MS;
    update_feature_cache(frame_time_);
}

Color VoronoiShader::render(float x, float y) {
    // Boundary conversion in: UV floats -> Q16.16 fixed point.
    const int32_t fx = static_cast<int32_t>(x * FP_ONE);
    const int32_t fy = static_cast<int32_t>(y * FP_ONE);

    const int32_t cell_x = fx / CELL_SIZE;
    const int32_t cell_y = fy / CELL_SIZE;

    uint32_t nearest_sq = UINT32_MAX;
    int32_t owner_cx = 0;
    int32_t owner_cy = 0;

    for (int32_t dy = -1; dy <= 1; ++dy) {
        for (int32_t dx = -1; dx <= 1; ++dx) {
            const int32_t ncx = cell_x + dx;
            const int32_t ncy = cell_y + dy;

            int32_t point_x, point_y;
            if (ncx >= -1 && ncx < FEATURE_CACHE_SIZE - 1 &&
                ncy >= -1 && ncy < FEATURE_CACHE_SIZE - 1) {
                const uint8_t cache_x = static_cast<uint8_t>(ncx + 1);
                const uint8_t cache_y = static_cast<uint8_t>(ncy + 1);
                point_x = feature_x_[cache_y][cache_x];
                point_y = feature_y_[cache_y][cache_x];
            } else {
                feature_point(ncx, ncy, frame_time_, &point_x, &point_y);
            }

            const int32_t ddx = fx - point_x;
            const int32_t ddy = fy - point_y;
            const uint32_t dist_sq = static_cast<uint32_t>(ddx * ddx + ddy * ddy);

            if (dist_sq < nearest_sq) {
                nearest_sq = dist_sq;
                owner_cx = ncx;
                owner_cy = ncy;
            }
        }
    }

    // Brighter near each cell's center.
    uint32_t glow_index = nearest_sq >> GLOW_LUT_SHIFT;
    if (glow_index >= GLOW_LUT_SIZE) {
        glow_index = GLOW_LUT_SIZE - 1;
    }
    const int32_t value_fp = glow_lut_[glow_index];

    const float value = static_cast<float>(value_fp) / static_cast<float>(FP_ONE);

    if (owner_cx >= -1 && owner_cx < FEATURE_CACHE_SIZE - 1 &&
        owner_cy >= -1 && owner_cy < FEATURE_CACHE_SIZE - 1) {
        const uint8_t cache_x = static_cast<uint8_t>(owner_cx + 1);
        const uint8_t cache_y = static_cast<uint8_t>(owner_cy + 1);
        return Color{
            static_cast<float>(color_r_[cache_y][cache_x]) * value / 255.0f,
            static_cast<float>(color_g_[cache_y][cache_x]) * value / 255.0f,
            static_cast<float>(color_b_[cache_y][cache_x]) * value / 255.0f,
        };
    }

    const uint32_t hue_hash = hash2(owner_cx, owner_cy, 3);
    const float hue = static_cast<float>(hue_hash % 360u);
    uint8_t r, g, b;
    hsv_to_rgb(hue, 1.0f, value, &r, &g, &b);
    return Color{r / 255.0f, g / 255.0f, b / 255.0f};
}
