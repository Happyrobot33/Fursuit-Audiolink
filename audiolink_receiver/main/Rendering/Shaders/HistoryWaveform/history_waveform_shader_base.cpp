#include "history_waveform_shader_base.h"

#include "shader_globals.h"

namespace {
constexpr float EDGE_SOFTNESS = 1.0f / 32.0f;
}

Color HistoryWaveformShaderBase::render(float x, float y) {
    const std::vector<float> &band = shader_audio_data().history.*band_;
    if (band.empty()) {
        return Color{0.0f, 0.0f, 0.0f};
    }

    const size_t sample_index = static_cast<size_t>(x * static_cast<float>(band.size() - 1) + 0.5f);
    const float value = band[sample_index];

    const float distance_to_edge = y + EDGE_SOFTNESS * 0.5f - value;
    const float unclamped_coverage = 0.5f - distance_to_edge / EDGE_SOFTNESS;
    const float coverage = unclamped_coverage <= 0.0f ? 0.0f :
        (unclamped_coverage >= 1.0f ? 1.0f : unclamped_coverage);

    return Color{color_.R * coverage, color_.G * coverage, color_.B * coverage};
}
