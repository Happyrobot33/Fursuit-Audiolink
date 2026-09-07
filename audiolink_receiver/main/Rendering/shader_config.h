#pragma once

// Selects which IShader implementations are used; to add a new shader, write it and
// change these lines to point at it. Kept out of config.h (included almost everywhere)
// so editing/adding a shader only recompiles shader_factory.cpp instead of the whole project.

// #include "chronotensity_rotating_square_shader.h"
// using SelectedShader = ChronotensityRotatingSquareShader;

// #include "bass_history_waveform_shader.h"
// using SelectedShader = BassHistoryWaveformShader;
// #include "lowmid_history_waveform_shader.h"
// using SelectedShader = LowMidHistoryWaveformShader;
// #include "highmid_history_waveform_shader.h"
// using SelectedShader = HighMidHistoryWaveformShader;
// #include "treble_history_waveform_shader.h"
// using SelectedShader = TrebleHistoryWaveformShader;

#include "stacked_history_waveform_shader.h"
using SelectedShader = StackedHistoryWaveformShader;

// #include "bass_history_brightness_shader.h"
// using SelectedShader = BassHistoryBrightnessShader;
// #include "lowmid_history_brightness_shader.h"
// using SelectedShader = LowMidHistoryBrightnessShader;
// #include "highmid_history_brightness_shader.h"
// using SelectedShader = HighMidHistoryBrightnessShader;
// #include "treble_history_brightness_shader.h"
// using SelectedShader = TrebleHistoryBrightnessShader;

// Shown at boot and whenever no audio data has arrived for FALLBACK_TIMEOUT_MS (see config.h).
// #include "rotating_square_shader.h"
// using FallbackShaderType = RotatingSquareShader;
#include "voronoi_shader.h"
using FallbackShaderType = VoronoiShader;
// #include "rainbow_shader.h"
// using FallbackShaderType = RainbowShader;
// #include "fallback_shader.h"
// using FallbackShaderType = FallbackShader;
