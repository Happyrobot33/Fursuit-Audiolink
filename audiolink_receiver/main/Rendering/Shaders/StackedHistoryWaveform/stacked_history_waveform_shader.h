#pragma once

#include "shader.h"

/** Renders bass, low-mid, high-mid, and treble history in four stacked rows. */
class StackedHistoryWaveformShader : public IShader {
public:
    Color render(float x, float y) override;
};
