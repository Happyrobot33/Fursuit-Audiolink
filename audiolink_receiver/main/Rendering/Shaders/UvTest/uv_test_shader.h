#pragma once

#include "shader.h"

/** Diagnostic shader that displays the incoming UV coordinates as red and green. */
class UvTestShader : public IShader {
public:
    Color render(float x, float y) override;
};
