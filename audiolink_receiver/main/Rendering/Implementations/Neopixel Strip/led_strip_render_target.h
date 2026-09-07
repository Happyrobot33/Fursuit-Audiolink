#pragma once

#include "render_target.h"
#include "led_strip.h"

class LEDController;

/** Treated as a single row (height() == 1); y is ignored when writing pixels. */
class LedStripRenderTarget : public IRenderTarget {
public:
    LedStripRenderTarget(LEDController& led_controller, led_strip_handle_t strip);

    uint16_t width() const override;
    uint16_t height() const override;
    void set_pixel(uint16_t x, uint16_t y, const Color& color) override;
    void clear() override;
    void present() override;

private:
    LEDController& led_controller_;
    led_strip_handle_t strip_;
};
