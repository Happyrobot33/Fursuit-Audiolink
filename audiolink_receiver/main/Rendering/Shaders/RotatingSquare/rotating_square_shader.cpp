#include "rotating_square_shader.h"

#include <cmath>

#include "shader_globals.h"

namespace {
constexpr float TWO_PI = 6.283185307179586f;
constexpr float ROTATIONS_PER_SECOND = 0.2f;
constexpr float HALF_SIDE_LENGTH = 0.28f;
constexpr float EDGE_SOFTNESS = 1.0f / 32.0f;
}

Color RotatingSquareShader::render(float x, float y) {
    static float cached_time = -1.0f;
    static float cosine = 1.0f;
    static float sine = 0.0f;

    if (_Time != cached_time) {
        const float angle = _Time * ROTATIONS_PER_SECOND * TWO_PI;
        cosine = std::cos(angle);
        sine = std::sin(angle);
        cached_time = _Time;
    }

    const float centered_x = x - 0.5f;
    const float centered_y = y - 0.5f;
    const float rotated_x = centered_x * cosine + centered_y * sine;
    const float rotated_y = -centered_x * sine + centered_y * cosine;
    const float distance_to_edge = std::fmax(std::abs(rotated_x), std::abs(rotated_y)) -
        HALF_SIDE_LENGTH;
    const float coverage = std::fmax(0.0f, std::fmin(1.0f,
        0.5f - distance_to_edge / EDGE_SOFTNESS));

    return Color{coverage, coverage, coverage};
}
