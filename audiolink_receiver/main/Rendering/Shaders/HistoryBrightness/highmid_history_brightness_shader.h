#pragma once

#include "history_brightness_shader_base.h"

/** High-mid band history waveform, tinted green. */
class HighMidHistoryBrightnessShader : public HistoryBrightnessShaderBase {
public:
    HighMidHistoryBrightnessShader() : HistoryBrightnessShaderBase(&History::highmid, Color{0.0f, 1.0f, 0.0f}) {}
};
