#pragma once

#include "shader.h"

/** A rotating square whose angle follows the filtered bass Chronotensity phase. */
class ChronotensityRotatingSquareShader : public IShader {
public:
    Color render(float x, float y) override;
};
