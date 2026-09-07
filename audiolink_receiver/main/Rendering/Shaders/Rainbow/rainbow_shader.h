#pragma once

#include "shader.h"

/** Placeholder shader: a rainbow sweeping across X, animated over time. */
class RainbowShader : public IShader {
public:
    Color render(float x, float y) override;
};
