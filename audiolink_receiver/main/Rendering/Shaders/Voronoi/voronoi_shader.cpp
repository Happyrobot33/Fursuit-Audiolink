#include "voronoi_shader.h"

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
constexpr int32_t EDGE_THRESHOLD = FP_ONE / 40;

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

// Binary integer square root (floor), avoids any float/double sqrt().
uint32_t isqrt64(uint64_t n) {
    uint64_t res = 0;
    uint64_t bit = 1ULL << 62;
    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<uint32_t>(res);
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

Color VoronoiShader::render(float x, float y) {
    // Boundary conversion in: UV floats -> Q16.16 fixed point.
    const int32_t fx = static_cast<int32_t>(x * FP_ONE);
    const int32_t fy = static_cast<int32_t>(y * FP_ONE);

    // Boundary conversion in: global _Time (seconds) -> integer milliseconds for the fixed-point phase.
    const uint32_t now_ms = static_cast<uint32_t>(_Time * 1000.0f);
    const uint32_t t = (static_cast<uint64_t>(now_ms % ANIM_PERIOD_MS) * FP_ONE) / ANIM_PERIOD_MS;

    const int32_t cell_x = fx / CELL_SIZE;
    const int32_t cell_y = fy / CELL_SIZE;

    uint64_t nearest_sq = UINT64_MAX;
    uint64_t second_sq = UINT64_MAX;
    int32_t owner_cx = 0;
    int32_t owner_cy = 0;

    for (int32_t dy = -1; dy <= 1; ++dy) {
        for (int32_t dx = -1; dx <= 1; ++dx) {
            const int32_t ncx = cell_x + dx;
            const int32_t ncy = cell_y + dy;

            int32_t point_x, point_y;
            feature_point(ncx, ncy, t, &point_x, &point_y);

            const int64_t ddx = fx - point_x;
            const int64_t ddy = fy - point_y;
            const uint64_t dist_sq = static_cast<uint64_t>(ddx * ddx + ddy * ddy);

            if (dist_sq < nearest_sq) {
                second_sq = nearest_sq;
                nearest_sq = dist_sq;
                owner_cx = ncx;
                owner_cy = ncy;
            } else if (dist_sq < second_sq) {
                second_sq = dist_sq;
            }
        }
    }

    const int32_t nearest_dist = static_cast<int32_t>(isqrt64(nearest_sq));
    const int32_t second_dist = static_cast<int32_t>(isqrt64(second_sq));
    const int32_t edge_gap = clamp_fp(second_dist - nearest_dist, 0, EDGE_THRESHOLD);

    // Brighter near each cell's center, darkened toward cell borders.
    const int32_t glow = FP_ONE - clamp_fp((nearest_dist * FP_ONE) / (CELL_SIZE * 3 / 2), 0, FP_ONE / 2);
    const int32_t edge_atten = (edge_gap * FP_ONE) / EDGE_THRESHOLD; // 0 at border, FP_ONE away from it
    const int32_t value_fp = clamp_fp((glow * edge_atten) / FP_ONE, FP_ONE / 8, FP_ONE);

    const uint32_t hue_hash = hash2(owner_cx, owner_cy, 3);
    const float hue = static_cast<float>(hue_hash % 360u);
    const float value = static_cast<float>(value_fp) / static_cast<float>(FP_ONE);

    uint8_t r, g, b;
    hsv_to_rgb(hue, 0.8f, value, &r, &g, &b);
    return Color{r / 255.0f, g / 255.0f, b / 255.0f};
}
