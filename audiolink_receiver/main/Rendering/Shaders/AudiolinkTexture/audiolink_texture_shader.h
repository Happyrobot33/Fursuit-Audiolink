#pragma once

#include <vector>

#include "shader.h"

/** Template for rendering a texture sourced from AudioLink data. */
class AudiolinkTextureShader : public IShader {
public:
    void begin_frame() override;
    Color render(float x, float y) override;

private:
    std::vector<Color> playerNameCol_;
    std::vector<Color> masterNameCol_;
    std::vector<Color> custom1Col_;
    std::vector<Color> custom2Col_;
};
