#pragma once

#include "audiolink_data.h"

/**
 * @class IShader
 * @brief Called once per pixel on the active render target.
 *
 * x and y are UV coordinates in [0, 1]; on an LED strip, y is always 0.
 * Implementations can also read the global `_Time` (seconds since boot) and
 * `shader_audio_data()` (latest decoded frame) declared in shader_globals.h.
 */
class IShader {
public:
    virtual ~IShader() = default;
    virtual Color render(float x, float y) = 0;
};
