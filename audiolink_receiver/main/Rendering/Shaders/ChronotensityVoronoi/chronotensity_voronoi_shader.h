#pragma once

#include "shader.h"

/** Animated cellular/Voronoi pattern restricted to the 4 theme colors, driven by Chronotensity phase instead of wall-clock time; all distance/hash math is done in Q16.16 fixed-point. */
class ChronotensityVoronoiShader : public IShader {
public:
    Color render(float x, float y) override;
};
