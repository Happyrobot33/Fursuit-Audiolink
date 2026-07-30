#pragma once

#include <array>
#include <vector>
#include "led_strip.h"
#include "audiolink_data.h"

/**
 * @class LEDController
 * @brief Manages LED strip control and frequency band visualization
 */
class LEDController {
public:
    static constexpr int LED_COUNT = 60;
    
    LEDController();
    
    /**
     * Initialize and configure the LED strip
     * @return Handle to the configured LED strip
     */
    led_strip_handle_t init();
    
    /**
     * Map frequency values to an LED quadrant with specified color
     * @param led_strip LED strip handle
     * @param frequency_values Vector of frequency values (0.0-1.0)
     * @param quadrant LED quadrant (0-3)
     * @param color Color to apply (float values 0.0-1.0)
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t map_to_leds(led_strip_handle_t led_strip, 
                          const std::vector<float>& frequency_values,
                          uint8_t quadrant, const Color& color);
    
    /**
     * Clear all LED pixels
     * @param led_strip LED strip handle
     */
    void clear(led_strip_handle_t led_strip);
    
    /**
     * Fill all LED pixels with a solid color
     * @param led_strip LED strip handle
     * @param color Color to fill with (float values 0.0-1.0)
     */
    void fill(led_strip_handle_t led_strip, const Color& color);
    
private:
    std::array<Color, LED_COUNT> pixels;
};

/**
 * Set an LED pixel using a Color struct (float values 0.0-1.0)
 * @param led_strip LED strip handle
 * @param index LED index
 * @param color Color to set (float values 0.0-1.0)
 * @return ESP_OK on success, ESP_FAIL on error
 */
inline esp_err_t led_strip_set_pixel_color(led_strip_handle_t led_strip, uint32_t index, const Color& color) {
    uint8_t r = (uint8_t)(color.r * 255.0f);
    uint8_t g = (uint8_t)(color.g * 255.0f);
    uint8_t b = (uint8_t)(color.b * 255.0f);
    return led_strip_set_pixel(led_strip, index, r, g, b);
}

/**
 * Convert HSV color to RGB
 * @param hue Hue angle (0-360)
 * @param saturation Saturation (0.0-1.0)
 * @param value Value/Brightness (0.0-1.0)
 * @param r Output red channel
 * @param g Output green channel
 * @param b Output blue channel
 */
[[maybe_unused]] void hsv_to_rgb(float hue, float saturation, float value, 
                                 uint8_t *r, uint8_t *g, uint8_t *b);
