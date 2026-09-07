#include "led_strip_render_target.h"

#include "esp_err.h"
#include "led_controller.h"
#include "config.h"

LedStripRenderTarget::LedStripRenderTarget(LEDController& led_controller, led_strip_handle_t strip)
    : led_controller_(led_controller), strip_(strip) {}

uint16_t LedStripRenderTarget::width() const { return LED_STRIP_LED_NUMBERS; }
uint16_t LedStripRenderTarget::height() const { return 1; }

void LedStripRenderTarget::set_pixel(uint16_t x, uint16_t /*y*/, const Color& color) {
    led_controller_.set_pixel(strip_, x, color);
}

void LedStripRenderTarget::clear() { led_controller_.clear(strip_); }
void LedStripRenderTarget::present() { ESP_ERROR_CHECK(led_strip_refresh(strip_)); }
