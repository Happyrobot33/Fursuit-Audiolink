#include "solid_test_shader.h"

// Flat magenta fill; only exists to prove new shader files get picked up automatically.
Color SolidTestShader::render(float x, float y) {
    return Color{1.0f, 0.0f, 1.0f};
}
