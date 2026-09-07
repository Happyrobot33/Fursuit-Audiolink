#pragma once

#include "shader.h"

/** Animated cellular/Voronoi pattern; all distance/hash math is done in Q16.16 fixed-point. */
class VoronoiShader : public IShader {
public:
    Color render(float x, float y) override;
};
