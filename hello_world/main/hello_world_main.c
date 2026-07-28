/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
static const char *TAG = "hello_world";
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include <math.h>
#include "esp_timer.h"     // Required for esp_timer_get_time()

#include "driver/gpio.h"
#include <led_strip_types.h>
#include <led_strip_rmt.h>
#include "led_strip.h"
#include "esp_log.h"

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  GPIO_NUM_13
// Numbers of the LED in the strip
#define LED_STRIP_LED_NUMBERS 60
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .color_component_format  = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void app_main(void)
{
    gpio_set_drive_capability(LED_STRIP_BLINK_GPIO, GPIO_DRIVE_CAP_3);
    led_strip_handle_t led_strip = configure_led();

    while (1) {
        //use time
        int64_t micro_seconds = esp_timer_get_time();
        float t = (float)micro_seconds / 1000000.0;
        const float FREQ = 0.2f; // Frequency in Hz (0.2 Hz = 5-second breath cycle)
        int wave = (int)(127.5 * sin(2 * M_PI * FREQ * t) + 127.5);

        for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, wave / 5, 0, 0));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(pdMS_TO_TICKS(20));  // ~50fps for smooth animation
    }


    fflush(stdout);
    esp_restart();
}
