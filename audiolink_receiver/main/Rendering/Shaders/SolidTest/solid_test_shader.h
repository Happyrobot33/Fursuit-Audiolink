#pragma once

#include "shader.h"

/** Dummy shader used to verify the CMake source glob auto-detects new files. */
class SolidTestShader : public IShader {
public:
    Color render(float x, float y) override;
};
