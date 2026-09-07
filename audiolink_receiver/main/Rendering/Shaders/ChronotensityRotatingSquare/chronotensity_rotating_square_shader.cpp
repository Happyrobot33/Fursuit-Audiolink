#include "chronotensity_rotating_square_shader.h"

#include <cmath>
#include <cstdint>

#include "shader_globals.h"

namespace {
constexpr float TWO_PI = 6.283185307179586f;
constexpr float HALF_SIDE_LENGTH = 0.28f;
constexpr float EDGE_SOFTNESS = 1.0f / 32.0f;
}

Color ChronotensityRotatingSquareShader::render(float x, float y) {
    static uint32_t cached_phase = UINT32_MAX;
    static float cosine = 1.0f;
    static float sine = 0.0f;

    const uint32_t phase = shader_audio_data().chronotensity.bass.increasing;
    if (phase != cached_phase) {
        const float angle = static_cast<float>(phase) / 1000000.0f * TWO_PI;
        cosine = std::cos(angle);
        sine = std::sin(angle);
        cached_phase = phase;
    }

    const float centered_x = x - 0.5f;
    const float centered_y = y - 0.5f;
    const float rotated_x = centered_x * cosine + centered_y * sine;
    const float rotated_y = -centered_x * sine + centered_y * cosine;
    const float absolute_x = rotated_x < 0.0f ? -rotated_x : rotated_x;
    const float absolute_y = rotated_y < 0.0f ? -rotated_y : rotated_y;
    const float distance_to_edge = (absolute_x > absolute_y ? absolute_x : absolute_y) -
        HALF_SIDE_LENGTH;
    const float unclamped_coverage = 0.5f - distance_to_edge / EDGE_SOFTNESS;
    const float coverage = unclamped_coverage <= 0.0f ? 0.0f :
        (unclamped_coverage >= 1.0f ? 1.0f : unclamped_coverage);
    const Color& theme_color = shader_audio_data().theme_colors.ThemeColor0;

    return Color{theme_color.R * coverage, theme_color.G * coverage, theme_color.B * coverage};
}
