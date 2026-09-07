#include "rainbow_shader.h"

#include <cmath>
#include "shader_globals.h"
#include "color_utils.h"

Color RainbowShader::render(float x, float y) {
    static constexpr float DEGREES_PER_SEC = 360.0f / 4.0f; // full hue cycle every 4 seconds
    const float hue = std::fmod(x * 360.0f + _Time * DEGREES_PER_SEC, 360.0f);
    const float value = 0.5f + 0.5f * y;

    uint8_t r, g, b;
    hsv_to_rgb(hue, 1.0f, value, &r, &g, &b);
    return Color{r / 255.0f, g / 255.0f, b / 255.0f};
}
