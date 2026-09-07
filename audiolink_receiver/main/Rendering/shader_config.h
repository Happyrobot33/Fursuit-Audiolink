#pragma once

// Selects which IShader implementations are used; to add a new shader, write it and
// change these lines to point at it. Kept out of config.h (included almost everywhere)
// so editing/adding a shader only recompiles shader_factory.cpp instead of the whole project.

#include "chronotensity_rotating_square_shader.h"
using SelectedShader = ChronotensityRotatingSquareShader;

// Shown at boot and whenever no audio data has arrived for FALLBACK_TIMEOUT_MS (see config.h).
#include "rotating_square_shader.h"
using FallbackShaderType = RotatingSquareShader;
// #include "voronoi_shader.h"
// using FallbackShaderType = VoronoiShader;
// #include "rainbow_shader.h"
// using FallbackShaderType = RainbowShader;
