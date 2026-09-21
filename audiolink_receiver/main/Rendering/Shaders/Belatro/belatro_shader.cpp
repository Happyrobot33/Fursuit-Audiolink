#include "belatro_shader.h"

#include <cmath>

#include "shader_globals.h"
#include "shader_math.h"

using ShaderMath::Vec2;
using ShaderMath::Vec3;
using ShaderMath::clamp01;
using ShaderMath::length;

Color BelatroShader::render(float x, float y) {
    constexpr float SPIN_ROTATION = -2.0f;
    constexpr float SPIN_SPEED = 7.0f;
    constexpr Vec2 OFFSET{0.0f, 0.0f};
    constexpr Vec3 COLOUR_1{0.871f, 0.267f, 0.231f};
    constexpr Vec3 COLOUR_2{0.0f, 0.42f, 0.706f};
    constexpr Vec3 COLOUR_3{0.086f, 0.137f, 0.145f};
    constexpr float CONTRAST = 3.5f;
    constexpr float LIGTHING = 0.4f;
    constexpr float SPIN_AMOUNT = 0.25f;
    constexpr float PIXEL_FILTER = 745.0f;
    constexpr float SPIN_EASE = 1.0f;
    constexpr bool IS_ROTATE = false;

    const Vec2 screenSize{1.0f, 1.0f};
    const float pixel_size = length(screenSize) / PIXEL_FILTER;
    Vec2 uv = {
        (std::floor(x / pixel_size) * pixel_size - 0.5f * screenSize.x) / length(screenSize) - OFFSET.x,
        (std::floor(y / pixel_size) * pixel_size - 0.5f * screenSize.y) / length(screenSize) - OFFSET.y,
    };

    float uv_len = length(uv);

    float speed = (SPIN_ROTATION * SPIN_EASE * 0.2f);
    if (IS_ROTATE) {
        speed = _Time * speed;
    }
    speed += 302.2f;

    const float new_pixel_angle = std::atan2(uv.y, uv.x) + speed - SPIN_EASE * 20.0f *
        (1.0f * SPIN_AMOUNT * uv_len + (1.0f - 1.0f * SPIN_AMOUNT));

    const Vec2 mid = {screenSize.x / length(screenSize) / 2.0f,
                      screenSize.y / length(screenSize) / 2.0f};

    uv = {
        (uv_len * std::cos(new_pixel_angle) + mid.x) - mid.x,
        (uv_len * std::sin(new_pixel_angle) + mid.y) - mid.y,
    };

    uv = uv * 30.0f;
    speed = _Time * SPIN_SPEED;
    Vec2 uv2{uv.x + uv.y, uv.x + uv.y};

    for (int i = 0; i < 5; ++i) {
        uv2.x += std::sin(std::max(uv.x, uv.y)) + uv.x;
        uv2.y += std::sin(std::max(uv.x, uv.y)) + uv.y;

        uv += 0.5f * Vec2{
            std::cos(5.1123314f + 0.353f * uv2.y + speed * 0.131121f),
            std::sin(uv2.x - 0.113f * speed),
        };

        const float twist = 1.0f * std::cos(uv.x + uv.y) - 1.0f * std::sin(uv.x * 0.711f - uv.y);
        uv.x -= twist;
        uv.y -= twist;
    }

    const float contrast_mod = (0.25f * CONTRAST + 0.5f * SPIN_AMOUNT + 1.2f);
    const float paint_res = std::min(2.0f, std::max(0.0f, length(uv) * 0.035f * contrast_mod));
    const float c1p = std::max(0.0f, 1.0f - contrast_mod * std::abs(1.0f - paint_res));
    const float c2p = std::max(0.0f, 1.0f - contrast_mod * std::abs(paint_res));
    const float c3p = 1.0f - std::min(1.0f, c1p + c2p);
    const float light = (LIGTHING - 0.2f) * std::max(c1p * 5.0f - 4.0f, 0.0f) +
                       LIGTHING * std::max(c2p * 5.0f - 4.0f, 0.0f);

    const Vec3 mixed = (0.3f / CONTRAST) * COLOUR_1 +
        (1.0f - 0.3f / CONTRAST) * (COLOUR_1 * c1p + COLOUR_2 * c2p + COLOUR_3 * c3p);
    const Vec3 final_color = mixed + Vec3{light, light, light};

    return Color{
        final_color.x,
        final_color.y,
        final_color.z,
    };
}
