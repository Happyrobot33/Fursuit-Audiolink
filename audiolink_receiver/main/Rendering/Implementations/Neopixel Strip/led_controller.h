#pragma once

#include <array>
#include <vector>
#include "led_strip.h"
#include "audiolink_data.h"
#include "config.h"

/**
 * @class LEDController
 * @brief Manages LED strip control and frequency band visualization
 */
class LEDController {
public:
    LEDController();
    
    /**
     * Initialize and configure the LED strip
     * @return Handle to the configured LED strip
     */
    led_strip_handle_t init();
    
    /**
     * Map frequency values to an LED pixel range with specified color
     * @param led_strip LED strip handle
     * @param frequency_values Vector of float frequency values (0.0-1.0 range)
     * @param start_pixel Starting LED index
     * @param end_pixel Ending LED index (exclusive)
     * @param color Color to apply (uint32_t values converted to 0.0-1.0 range)
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t map_to_leds(led_strip_handle_t led_strip, 
                          const std::vector<float>& frequency_values,
                          uint16_t start_pixel, uint16_t end_pixel, const Color& color);
    
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

    /**
     * Set a specific LED pixel to a color
     * @param led_strip LED strip handle
     * @param index LED index (0 to LED_STRIP_LED_NUMBERS-1)
     * @param color Color to set (float values 0.0-1.0)
     */
    void set_pixel(led_strip_handle_t led_strip, int index, const Color& color);
    
private:
    std::array<Color, LED_STRIP_LED_NUMBERS> pixels;
};

/**
 * Set an LED pixel using a Color struct (float values 0.0-1.0)
 * @param led_strip LED strip handle
 * @param index LED index
 * @param color Color to set (float values 0.0-1.0)
 * @return ESP_OK on success, ESP_FAIL on error
 */
inline esp_err_t led_strip_set_pixel_color(led_strip_handle_t led_strip, uint32_t index, const Color& color) {
    uint8_t r = (uint8_t)(color.R * 255.0f);
    uint8_t g = (uint8_t)(color.G * 255.0f);
    uint8_t b = (uint8_t)(color.B * 255.0f);
    return led_strip_set_pixel(led_strip, index, r, g, b);
}

