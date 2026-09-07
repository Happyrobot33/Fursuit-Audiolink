#pragma once

#include "shader.h"

/** Idle animation shown at boot and whenever no audio data has arrived recently. */
class FallbackShader : public IShader {
public:
    Color render(float x, float y) override;
};
