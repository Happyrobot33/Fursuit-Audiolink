#include "chronotensity_voronoi_shader.h"

#include <cstdint>
#include "shader_globals.h"

namespace {

// Q16.16 fixed point: all cell/hash/distance math below stays in this integer format;
// the only float conversions are the UV input and the final Color blend output.
constexpr int32_t FP_SHIFT = 16;
constexpr int32_t FP_ONE = 1 << FP_SHIFT;

constexpr int32_t GRID_SIZE = 8;              // feature cells across the UV space
constexpr int32_t CELL_SIZE = FP_ONE / GRID_SIZE;
constexpr int32_t WOBBLE_AMPLITUDE = CELL_SIZE / 4;

// Chronotensity phase is a 0..1,000,000 counter representing one full animation cycle.
constexpr uint32_t CHRONOTENSITY_PERIOD = 1000000u;

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

Color ChronotensityVoronoiShader::render(float x, float y) {
    // Boundary conversion in: UV floats -> Q16.16 fixed point.
    const int32_t fx = static_cast<int32_t>(x * FP_ONE);
    const int32_t fy = static_cast<int32_t>(y * FP_ONE);

    // Boundary conversion in: Chronotensity phase (0..1,000,000) -> Q16.16 fixed-point phase.
    const uint32_t phase = shader_audio_data().chronotensity.bass.increasing;
    const uint32_t t = static_cast<uint32_t>((static_cast<uint64_t>(phase) * FP_ONE) / CHRONOTENSITY_PERIOD);

    const int32_t cell_x = fx / CELL_SIZE;
    const int32_t cell_y = fy / CELL_SIZE;

    uint64_t nearest_sq = UINT64_MAX;
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
                nearest_sq = dist_sq;
                owner_cx = ncx;
                owner_cy = ncy;
            }
        }
    }

    const int32_t nearest_dist = static_cast<int32_t>(isqrt64(nearest_sq));

    // Brighter near each cell's center.
    const int32_t glow = FP_ONE - clamp_fp((nearest_dist * FP_ONE) / (CELL_SIZE * 3 / 2), 0, FP_ONE / 2);
    const float value = static_cast<float>(glow) / static_cast<float>(FP_ONE);

    // Pick one of the 4 theme colors per cell instead of a full hue wheel.
    const ThemeColors &theme_colors = shader_audio_data().theme_colors;
    const Color *palette[] = {
        &theme_colors.ThemeColor0,
        &theme_colors.ThemeColor1,
        &theme_colors.ThemeColor2,
        &theme_colors.ThemeColor3,
    };
    const uint32_t color_hash = hash2(owner_cx, owner_cy, 3);
    const Color &cell_color = *palette[color_hash % 4];

    return Color{cell_color.R * value, cell_color.G * value, cell_color.B * value};
}
