#pragma once

#include "history_brightness_shader_base.h"

/** Treble band history waveform, tinted blue. */
class TrebleHistoryBrightnessShader : public HistoryBrightnessShaderBase {
public:
    TrebleHistoryBrightnessShader() : HistoryBrightnessShaderBase(&History::treble, Color{0.0f, 0.0f, 1.0f}) {}
};
