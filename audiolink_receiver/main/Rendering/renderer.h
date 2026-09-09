#pragma once

#include "render_target.h"
#include "shader.h"
#include "shader_config.h"
#include "audiolink_data.h"

/** Refreshes shader globals with audio_data, then samples the shader across target's UV space (0..1) and pushes the resulting colors.
 * The fit mode controls how a non-square target is mapped into the shader's UV space, and the alignment controls how content is anchored.
 */
void render_shader_frame(IRenderTarget& target, IShader& shader, const AudiolinkData& audio_data,
                        UvFitMode fit_mode, UvAlignment alignment, UvRepeatMode repeat_mode);
