#include "belatro_shader.h"

#include <cmath>

#include "shader_globals.h"
#include "shader_math.h"
#include "belatro_trig_tables.h"

using ShaderMath::Vec2;
using ShaderMath::clamp01;
using ShaderMath::length;

namespace {

inline float fast_sin(float angle) {
    constexpr float TWO_PI = 6.2831853072f;
    constexpr float TABLE_SCALE = 256.0f / TWO_PI;

    float table_position = angle * TABLE_SCALE;
    table_position -= std::floor(table_position / 256.0f) * 256.0f;

    const int index = static_cast<int>(table_position);
    const float fraction = table_position - static_cast<float>(index);
    const float first = BelatroTrigTables::SIN_TABLE[index];
    const float second = BelatroTrigTables::SIN_TABLE[(index + 1) & 255];
    return first + (second - first) * fraction;
}

inline float fast_cos(float angle) {
    constexpr float HALF_PI = 1.5707963268f;
    return fast_sin(angle + HALF_PI);
}

inline float fast_atan2(float y, float x) {
    constexpr float PI = 3.1415926536f;
    constexpr float HALF_PI = 1.5707963268f;
    constexpr float ATAN_TABLE_SCALE = 128.0f;

    const float absolute_x = std::abs(x);
    const float absolute_y = std::abs(y);
    const bool y_dominant = absolute_y > absolute_x;
    const float ratio = y_dominant
        ? absolute_x / (absolute_y + 1.0e-10f)
        : absolute_y / (absolute_x + 1.0e-10f);
    const float table_position = ratio * ATAN_TABLE_SCALE;
    const int index = static_cast<int>(table_position);
    const float fraction = table_position - static_cast<float>(index);
    const float first = BelatroTrigTables::ATAN_TABLE[index];
    const float second = BelatroTrigTables::ATAN_TABLE[index + (index < 128 ? 1 : 0)];
    float angle = first + (second - first) * fraction;

    if (y_dominant) {
        angle = HALF_PI - angle;
    }
    if (x < 0.0f) {
        angle = PI - angle;
    }
    return y < 0.0f ? -angle : angle;
}

}  // namespace

void BelatroShader::begin_frame() {
    speed_ = _Time * 7.0f;
    cosine_phase_ = speed_ * 0.131121f;
    sine_phase_ = speed_ * 0.113f;
}

Color BelatroShader::render(float x, float y) {
    constexpr Color COLOUR_1{0.871f, 0.267f, 0.231f};
    constexpr Color COLOUR_2{0.0f, 0.42f, 0.706f};
    constexpr Color COLOUR_3{0.086f, 0.137f, 0.145f};
    constexpr float LIGTHING = 0.4f;
    constexpr float CONTRAST_MOD = 2.2f;
    constexpr float PAINT_SCALE = 0.0385f;
    constexpr float COLOUR_BASE = 0.08571429f;
    constexpr float COLOUR_REMAINDER = 0.91428571f;

    Vec2 uv = {
        x - 0.5f,
        y - 0.5f,
    };

    const float uv_len = length(uv);
    const float speed = 301.8f;

    const float new_pixel_angle = fast_atan2(uv.y, uv.x) + speed - 5.0f * uv_len - 15.0f;

    uv = {
        uv_len * fast_cos(new_pixel_angle),
        uv_len * fast_sin(new_pixel_angle),
    };

    uv = uv * 30.0f;
    Vec2 uv2{uv.x + uv.y, uv.x + uv.y};

    #pragma GCC unroll 2
    for (int i = 0; i < 2; ++i) {
        const float shared_wave = fast_sin(std::max(uv.x, uv.y));
        uv2.x += shared_wave + uv.x;
        uv2.y += shared_wave + uv.y;

        uv += 0.5f * Vec2{
            fast_cos(5.1123314f + 0.353f * uv2.y + cosine_phase_),
            fast_sin(uv2.x - sine_phase_),
        };

        const float twist = fast_cos(uv.x + uv.y) - fast_sin(uv.x * 0.711f - uv.y);
        uv.x -= twist;
        uv.y -= twist;
    }

    const float paint_res = clamp01(length(uv) * PAINT_SCALE) * 2.0f;
    const float c1p = clamp01(1.0f - CONTRAST_MOD * std::abs(1.0f - paint_res));
    const float c2p = clamp01(1.0f - CONTRAST_MOD * std::abs(paint_res));
    const float c3p = 1.0f - clamp01(c1p + c2p);
    const float light = (LIGTHING - 0.2f) * clamp01(c1p * 5.0f - 4.0f) +
                       LIGTHING * clamp01(c2p * 5.0f - 4.0f);

    const Color mixed = COLOUR_BASE * COLOUR_1 +
        COLOUR_REMAINDER * (COLOUR_1 * c1p + COLOUR_2 * c2p + COLOUR_3 * c3p);
    const Color final_color = mixed + Color{light, light, light};

    return final_color;
}
