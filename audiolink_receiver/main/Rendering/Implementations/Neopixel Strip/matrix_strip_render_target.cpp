#include "matrix_strip_render_target.h"

#include "esp_err.h"
#include "led_controller.h"
#include "config.h"

MatrixStripRenderTarget::MatrixStripRenderTarget(LEDController& led_controller, led_strip_handle_t strip)
    : led_controller_(led_controller), strip_(strip) {}

uint16_t MatrixStripRenderTarget::width() const { return MATRIX_WIDTH; }
uint16_t MatrixStripRenderTarget::height() const { return MATRIX_HEIGHT; }

void MatrixStripRenderTarget::set_pixel(uint16_t x, uint16_t y, const Color& color) {
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) {
        return;
    }

    const uint16_t mapped_y = (x % 2 == 1) ? (MATRIX_HEIGHT - 1 - y) : y;
    const uint16_t index = static_cast<uint16_t>(x) * MATRIX_HEIGHT + mapped_y;
    led_controller_.set_pixel(strip_, index, color);
}

void MatrixStripRenderTarget::clear() { led_controller_.clear(strip_); }
void MatrixStripRenderTarget::present() { ESP_ERROR_CHECK(led_strip_refresh(strip_)); }
