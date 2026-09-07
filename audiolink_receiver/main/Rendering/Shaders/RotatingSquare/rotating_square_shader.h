#pragma once

#include "shader.h"

/** A centered square that rotates continuously without audio data. */
class RotatingSquareShader : public IShader {
public:
    Color render(float x, float y) override;
};
