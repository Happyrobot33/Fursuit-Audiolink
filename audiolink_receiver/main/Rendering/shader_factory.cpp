#include "shader_factory.h"

#include "shader_config.h"

std::unique_ptr<IShader> create_shader() {
    return std::make_unique<SelectedShader>();
}

std::unique_ptr<IShader> create_fallback_shader() {
    return std::make_unique<FallbackShaderType>();
}
