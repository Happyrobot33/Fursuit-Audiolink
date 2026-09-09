#pragma once

#include "render_target.h"
#include "led_strip.h"

class LEDController;

/**
 * Treats a strip as a 2D matrix using the configured MATRIX_WIDTH and MATRIX_HEIGHT.
 * Pixel coordinates are mapped linearly to the strip so the strip behaves like a matrix.
 */
class MatrixStripRenderTarget : public IRenderTarget {
public:
    MatrixStripRenderTarget(LEDController& led_controller, led_strip_handle_t strip);

    uint16_t width() const override;
    uint16_t height() const override;
    void set_pixel(uint16_t x, uint16_t y, const Color& color) override;
    void clear() override;
    void present() override;

private:
    LEDController& led_controller_;
    led_strip_handle_t strip_;
};
