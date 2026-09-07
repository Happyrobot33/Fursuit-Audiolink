#pragma once

#include "history_brightness_shader_base.h"

/** Bass band history waveform, tinted red. */
class BassHistoryBrightnessShader : public HistoryBrightnessShaderBase {
public:
    BassHistoryBrightnessShader() : HistoryBrightnessShaderBase(&History::bass, Color{1.0f, 0.0f, 0.0f}) {}
};
