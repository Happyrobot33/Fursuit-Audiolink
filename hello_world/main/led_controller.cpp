#include <cmath>
#include <cstring>
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip_types.h"
#include "led_strip_rmt.h"
#include "led_controller.h"
#include "config.h"

LEDController::LEDController() {
    // Initialize all pixels to off
    for (auto& pixel : pixels) {
        pixel.r = 0;
        pixel.g = 0;
        pixel.b = 0;
    }
}

led_strip_handle_t LEDController::init() {
    led_strip_config_t strip_config;
    memset(&strip_config, 0, sizeof(strip_config));
    strip_config.strip_gpio_num = LED_STRIP_BLINK_GPIO;
    strip_config.max_leds = LED_STRIP_LED_NUMBERS;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.flags.invert_out = false;
    
    led_strip_rmt_config_t rmt_config;
    memset(&rmt_config, 0, sizeof(rmt_config));
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = LED_STRIP_RMT_RES_HZ;
    rmt_config.flags.with_dma = false;
    
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void hsv_to_rgb(float hue, float saturation, float value, uint8_t *r, uint8_t *g, uint8_t *b) {
    float c = value * saturation;
    float x = c * (1 - std::fabs(std::fmod(hue / 60.0, 2) - 1));
    float m = value - c;
    float rf, gf, bf;
    
    if (hue < 60) {
        rf = c; gf = x; bf = 0;
    } else if (hue < 120) {
        rf = x; gf = c; bf = 0;
    } else if (hue < 180) {
        rf = 0; gf = c; bf = x;
    } else if (hue < 240) {
        rf = 0; gf = x; bf = c;
    } else if (hue < 300) {
        rf = x; gf = 0; bf = c;
    } else {
        rf = c; gf = 0; bf = x;
    }
    
    *r = (uint8_t)((rf + m) * 255);
    *g = (uint8_t)((gf + m) * 255);
    *b = (uint8_t)((bf + m) * 255);
}

esp_err_t LEDController::map_to_leds(led_strip_handle_t led_strip, 
                                      const std::vector<float>& frequency_values,
                                      uint8_t quadrant, uint8_t r, uint8_t g, uint8_t b) {
    if (frequency_values.empty() || quadrant >= 4) {
        return ESP_FAIL;
    }
    
    int quadrant_size = LED_STRIP_LED_NUMBERS / 4;
    int start_led = quadrant * quadrant_size;
    int end_led = start_led + quadrant_size;
    
    for (int i = start_led; i < end_led; i++) {
        /* Map position within quadrant to frequency index */
        int quadrant_pos = i - start_led;
        size_t idx = (quadrant_pos * frequency_values.size()) / quadrant_size;
        if (idx >= frequency_values.size()) {
            idx = frequency_values.size() - 1;
        }
        
        float value = frequency_values[idx];
        if (value > 1.0) value = 1.0;
        if (value < 0.0) value = 0.0;
        
        /* Scale the color by the frequency value (brightness modulation) */
        uint8_t r_scaled = (uint8_t)(r * value);
        uint8_t g_scaled = (uint8_t)(g * value);
        uint8_t b_scaled = (uint8_t)(b * value);
        
        /* Update local pixel state and LED */
        pixels[i].r = r_scaled;
        pixels[i].g = g_scaled;
        pixels[i].b = b_scaled;
        
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, r_scaled, g_scaled, b_scaled));
    }
    
    return ESP_OK;
}

void LEDController::clear(led_strip_handle_t led_strip) {
    for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
        pixels[i].r = 0;
        pixels[i].g = 0;
        pixels[i].b = 0;
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 0, 0, 0));
    }
}
