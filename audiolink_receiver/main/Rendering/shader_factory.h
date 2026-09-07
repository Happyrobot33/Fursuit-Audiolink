#pragma once

#include <memory>
#include "shader.h"

/** Constructs the shader selected by SelectedShader in shader_config.h. */
std::unique_ptr<IShader> create_shader();

/** Constructs the idle shader shown at boot / when audio data goes stale. */
std::unique_ptr<IShader> create_fallback_shader();
