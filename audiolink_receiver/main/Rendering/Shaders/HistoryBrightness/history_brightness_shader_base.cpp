#include "history_brightness_shader_base.h"

#include "shader_globals.h"

namespace {
constexpr float EDGE_SOFTNESS = 1.0f / 32.0f;
}

Color HistoryBrightnessShaderBase::render(float x, float y) {
    const std::vector<float> &band = shader_audio_data().history.*band_;
    if (band.empty()) {
        return Color{0.0f, 0.0f, 0.0f};
    }

    const size_t sample_index = static_cast<size_t>(x * static_cast<float>(band.size() - 1) + 0.5f);
    const float value = band[sample_index];

    return Color{
        color_.R * value,
        color_.G * value,
        color_.B * value
    };
}
