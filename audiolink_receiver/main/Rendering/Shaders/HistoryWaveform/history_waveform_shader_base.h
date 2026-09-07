#pragma once

#include "shader.h"

/**
 * @class HistoryWaveformShaderBase
 * @brief Renders one History band as a bottom-up waveform: x selects the sample in time,
 * y is lit up to that sample's amplitude. Subclasses just supply which band (bass/lowmid/
 * highmid/treble) and tint color to use; the sampling/coverage math is shared here.
 */
class HistoryWaveformShaderBase : public IShader {
public:
    Color render(float x, float y) override;

protected:
    HistoryWaveformShaderBase(std::vector<float> History::*band, Color color)
        : band_(band), color_(color) {}

private:
    std::vector<float> History::*band_;
    Color color_;
};
