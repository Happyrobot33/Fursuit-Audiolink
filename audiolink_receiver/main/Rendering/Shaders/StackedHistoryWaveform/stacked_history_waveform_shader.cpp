#include "stacked_history_waveform_shader.h"

#include "shader_globals.h"

namespace {
constexpr float EDGE_SOFTNESS = 1.0f / 32.0f;
constexpr float ROW_COUNT = 4.0f;
}

Color StackedHistoryWaveformShader::render(float x, float y) {
    const float scaled_y = y * ROW_COUNT;
    const size_t row = scaled_y >= ROW_COUNT ? 3 : static_cast<size_t>(scaled_y);
    const float row_y = scaled_y - static_cast<float>(row);

    const History &history = shader_audio_data().history;
    const std::vector<float> *bands[] = {
        &history.bass,
        &history.lowmid,
        &history.highmid,
        &history.treble,
    };
    const Color colors[] = {
        Color{1.0f, 0.0f, 0.0f},
        Color{1.0f, 1.0f, 0.0f},
        Color{0.0f, 1.0f, 0.0f},
        Color{0.0f, 0.0f, 1.0f},
    };

    const std::vector<float> &band = *bands[row];
    if (band.empty()) {
        return Color{0.0f, 0.0f, 0.0f};
    }

    const size_t sample_index = static_cast<size_t>(x * static_cast<float>(band.size() - 1) + 0.5f);
    const float value = band[sample_index];
    const float distance_to_edge = row_y + EDGE_SOFTNESS * 0.5f - value;
    const float unclamped_coverage = 0.5f - distance_to_edge / EDGE_SOFTNESS;
    const float coverage = unclamped_coverage <= 0.0f ? 0.0f :
        (unclamped_coverage >= 1.0f ? 1.0f : unclamped_coverage);

    return Color{
        colors[row].R * coverage,
        colors[row].G * coverage,
        colors[row].B * coverage
    };
}
