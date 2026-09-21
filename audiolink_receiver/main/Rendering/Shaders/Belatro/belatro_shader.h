#pragma once

#include "shader.h"

/** Idle fallback animation inspired by Balatro's spin/paint effect. */
class BelatroShader : public IShader {
public:
    Color render(float x, float y) override;
};
