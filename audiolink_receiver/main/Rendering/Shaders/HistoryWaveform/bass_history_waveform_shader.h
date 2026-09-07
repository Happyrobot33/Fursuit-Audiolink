#pragma once

#include "history_waveform_shader_base.h"

/** Bass band history waveform, tinted red. */
class BassHistoryWaveformShader : public HistoryWaveformShaderBase {
public:
    BassHistoryWaveformShader() : HistoryWaveformShaderBase(&History::bass, Color{1.0f, 0.0f, 0.0f}) {}
};
