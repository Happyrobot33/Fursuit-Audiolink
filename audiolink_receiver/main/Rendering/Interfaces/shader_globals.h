#pragma once

#include "audiolink_data.h"

/**
 * Global state shared with every IShader, refreshed once per frame so render(x, y) doesn't
 * need to take extra parameters.
 */

/** Seconds since boot; updated once per rendered frame. */
extern float _Time;

/** Most recently decoded audio frame. */
const AudiolinkData &shader_audio_data();

/** Refreshes all shader globals (_Time, shader_audio_data(), ...) for the frame about to render. */
void update_shader_globals(const AudiolinkData &audio_data);
