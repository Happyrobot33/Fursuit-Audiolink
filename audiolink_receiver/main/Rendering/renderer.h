#pragma once

#include "render_target.h"
#include "shader.h"
#include "audiolink_data.h"

/** Refreshes shader globals with audio_data, then samples the shader across target's UV space (0..1) and pushes the resulting colors. */
void render_shader_frame(IRenderTarget& target, IShader& shader, const AudiolinkData& audio_data);
