#pragma once

#include "history_waveform_shader_base.h"

/** High-mid band history waveform, tinted green. */
class HighMidHistoryWaveformShader : public HistoryWaveformShaderBase {
public:
    HighMidHistoryWaveformShader() : HistoryWaveformShaderBase(&History::highmid, Color{0.0f, 1.0f, 0.0f}) {}
};
