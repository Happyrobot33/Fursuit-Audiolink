#pragma once

#include "render_target.h"

class MatrixController;

class MatrixRenderTarget : public IRenderTarget {
public:
    explicit MatrixRenderTarget(MatrixController& matrix);

    uint16_t width() const override;
    uint16_t height() const override;
    void set_pixel(uint16_t x, uint16_t y, const Color& color) override;
    void clear() override;
    void present() override;

private:
    MatrixController& matrix_;
};
