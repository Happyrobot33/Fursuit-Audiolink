#pragma once

#include "shader.h"

/**
 * Fallback shader that renders a static "NO SIGNAL" message using FontLookup, plus a
 * second line showing the elapsed time since boot (MM:SS.mmm), which updates every frame.
 */
class TextShader : public IShader {
public:
    void begin_frame() override;
    Color render(float x, float y) override;

private:
    char second_line_[10] = "00:00.000"; // "MM:SS.mmm", formatted once per frame in begin_frame()
    Color second_line_color_{1.0f, 1.0f, 1.0f}; // HSV-rotated once per frame in begin_frame()
};
