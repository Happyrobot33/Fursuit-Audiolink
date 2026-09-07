#include "fallback_shader.h"

#include <cmath>
#include "shader_globals.h"

namespace {
constexpr float TWO_PI = 6.283185307179586f;
constexpr float BREATH_HZ = 1.0f / 8.0f; // one breath every 8 seconds
} // namespace

// Slow dim blue "breathing" pulse; doesn't depend on audio data so it's safe with none.
Color FallbackShader::render(float x, float y) {
    static float cached_time = -1.0f;
    static float cached_value = 0.0f;

    if (_Time != cached_time) {
        const float breath = 0.5f + 0.5f * std::sin(_Time * BREATH_HZ * TWO_PI);
        cached_value = 0.35f + 0.45f * breath;
        cached_time = _Time;
    }

    return Color{0.0f, 0.35f * cached_value, cached_value};
}
