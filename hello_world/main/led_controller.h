#pragma once

#include <array>
#include <vector>
#include "led_strip.h"

/**
 * @struct Pixel
 * @brief RGB pixel representation
 */
struct Pixel {
    uint8_t r = 0, g = 0, b = 0;
};

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
     * @param r Red channel (0-255)
     * @param g Green channel (0-255)
     * @param b Blue channel (0-255)
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t map_to_leds(led_strip_handle_t led_strip, 
                          const std::vector<float>& frequency_values,
                          uint8_t quadrant, uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * Clear all LED pixels
     * @param led_strip LED strip handle
     */
    void clear(led_strip_handle_t led_strip);
    
private:
    std::array<Pixel, LED_COUNT> pixels;
};

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
