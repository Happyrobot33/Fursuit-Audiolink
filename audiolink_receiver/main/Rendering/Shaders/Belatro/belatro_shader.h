#pragma once

#include "shader.h"

/** Idle fallback animation inspired by Balatro's spin/paint effect. */
class BelatroShader : public IShader {
public:
    void begin_frame() override;
    Color render(float x, float y) override;

private:
    float speed_ = 0.0f;
    float cosine_phase_ = 0.0f;
    float sine_phase_ = 0.0f;
};
