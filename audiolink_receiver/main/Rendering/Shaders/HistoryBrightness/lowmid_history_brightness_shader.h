#pragma once

#include "history_brightness_shader_base.h"

/** Low-mid band history waveform, tinted yellow. */
class LowMidHistoryBrightnessShader : public HistoryBrightnessShaderBase {
public:
    LowMidHistoryBrightnessShader() : HistoryBrightnessShaderBase(&History::lowmid, Color{1.0f, 1.0f, 0.0f}) {}
};
