#pragma once

#include "history_waveform_shader_base.h"

/** Low-mid band history waveform, tinted yellow. */
class LowMidHistoryWaveformShader : public HistoryWaveformShaderBase {
public:
    LowMidHistoryWaveformShader() : HistoryWaveformShaderBase(&History::lowmid, Color{1.0f, 1.0f, 0.0f}) {}
};
