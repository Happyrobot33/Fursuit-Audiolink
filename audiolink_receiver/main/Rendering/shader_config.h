#pragma once

#include <cstdint>

// Set to 0 to compile out temporary render timing instrumentation.
#define RENDER_PROFILING_ENABLED 0

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
// #include "uv_test_shader.h"
// using SelectedShader = UvTestShader;

#include "stacked_history_waveform_shader.h"
using SelectedShader = StackedHistoryWaveformShader;

// #include "chronotensity_voronoi_shader.h"
// using SelectedShader = ChronotensityVoronoiShader;

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
// #include "uv_test_shader.h"
// using FallbackShaderType = UvTestShader;

// Common UV fit modes for mapping a pixel grid onto the shader's UV domain.
// - Stretch: use the native width/height mapping exactly.
// - Cover: preserve aspect ratio and cover the full area with the larger axis.
// - Tile: repeat the UV tile while using the selected alignment on the larger axis.
enum class UvFitMode : uint8_t {
    Stretch,
    Cover,
    Tile,
};

//these work for both cover and tile
//for cover, alignment determines how the smaller axis is positioned within the larger axis.
//for tile, alignment determines how the repeated tiles are positioned within the larger axis.
enum class UvAlignment : uint8_t {
    Left,
    Center,
    Right,
};

enum class UvRepeatMode : uint8_t {
    Blank,
    Flip,
    Repeat,
};

namespace SelectedShaderUVConfig {
static constexpr UvFitMode FIT_MODE = UvFitMode::Stretch;
static constexpr UvAlignment FIT_ALIGNMENT = UvAlignment::Center;
static constexpr UvRepeatMode REPEAT_MODE = UvRepeatMode::Repeat;
}

namespace FallbackShaderUVConfig {
static constexpr UvFitMode FIT_MODE = UvFitMode::Stretch;
static constexpr UvAlignment FIT_ALIGNMENT = UvAlignment::Center;
static constexpr UvRepeatMode REPEAT_MODE = UvRepeatMode::Repeat;
}

