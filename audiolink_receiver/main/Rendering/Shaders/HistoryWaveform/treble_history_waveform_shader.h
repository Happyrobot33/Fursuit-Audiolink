#pragma once

#include "history_waveform_shader_base.h"

/** Treble band history waveform, tinted blue. */
class TrebleHistoryWaveformShader : public HistoryWaveformShaderBase {
public:
    TrebleHistoryWaveformShader() : HistoryWaveformShaderBase(&History::treble, Color{0.0f, 0.0f, 1.0f}) {}
};
