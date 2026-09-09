#include "matrix_render_target.h"

#include "matrix_controller.h"
#include "config.h"

MatrixRenderTarget::MatrixRenderTarget(MatrixController& matrix) : matrix_(matrix) {}

uint16_t MatrixRenderTarget::width() const { return SCREEN_WIDTH; }
uint16_t MatrixRenderTarget::height() const { return SCREEN_HEIGHT; }

void MatrixRenderTarget::set_pixel(uint16_t x, uint16_t y, const Color& color) {
    matrix_.set_pixel(x, y, color);
}

void MatrixRenderTarget::clear() { matrix_.clear(); }
void MatrixRenderTarget::present() { matrix_.present(); }
