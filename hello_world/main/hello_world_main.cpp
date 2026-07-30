/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cstdio>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_controller.h"
#include "receiver.h"
#include "config.h"

// Global data structures
AudiolinkData audio_data;
bool audio_complete = false;
LEDController led_controller;
SemaphoreHandle_t audio_mutex = nullptr;

// Framerate tracking
static uint32_t frame_count = 0;
static uint32_t last_log_time_ms = 0;

extern "C" void app_main(void) {
    /* Initialize synchronization */
    audio_mutex = xSemaphoreCreateMutex();
    if (!audio_mutex) {
        ESP_LOGE(TAG, "Failed to create audio mutex");
        return;
    }

    /* Initialize GPIO and LED strip */
    gpio_set_drive_capability(LED_STRIP_BLINK_GPIO, GPIO_DRIVE_CAP_3);
    led_strip_handle_t led_strip = led_controller.init();

    /* Initialize WiFi and ESP-NOW */
    receiver_wifi_init();
    receiver_espnow_init();

    ESP_LOGI(TAG, "Application started - waiting for audio data...");

    /* Main loop */
    while (1) {
        uint32_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            if (audio_complete) {
                // Calculate and log framerate every second
                frame_count++;
                uint32_t elapsed_time_ms = current_time_ms - last_log_time_ms;
                if (elapsed_time_ms >= 1000) {
                    float framerate = (frame_count * 1000.0f) / elapsed_time_ms;
                    ESP_LOGI(TAG, "Framerate: %.1f FPS", framerate);
                    frame_count = 0;
                    last_log_time_ms = current_time_ms;
                }
                /* Clear LED strip */
                led_controller.clear(led_strip);

                //print what theme color 0 is
                // ESP_LOGI(TAG, "Theme Color 1: R=%.2f, G=%.2f, B=%.2f",
                //          audio_data.theme_colors.ThemeColor1.r,
                //          audio_data.theme_colors.ThemeColor1.g,
                //          audio_data.theme_colors.ThemeColor1.b);

                //fill with theme color
                // led_controller.fill(led_strip, audio_data.theme_colors.ThemeColor0);
                led_controller.set_pixel(led_strip, 10 + 0, audio_data.theme_colors.ThemeColor0);
                led_controller.set_pixel(led_strip, 10 + 1, audio_data.theme_colors.ThemeColor1);
                led_controller.set_pixel(led_strip, 10 + 2, audio_data.theme_colors.ThemeColor2);
                led_controller.set_pixel(led_strip, 10 + 3, audio_data.theme_colors.ThemeColor3);

                // /* Map 4 frequency bands to 4 LED quadrants */
                // /* Quadrant 0: bass -> red */
                // if (!audio_data.history.bass.empty()) {
                //     led_controller.map_to_leds(led_strip, audio_data.history.bass, 0, Color{1.0f, 0.0f, 0.0f});
                // }
                
                // /* Quadrant 1: lowmid -> yellow */
                // if (!audio_data.history.lowmid.empty()) {
                //     led_controller.map_to_leds(led_strip, audio_data.history.lowmid, 1, Color{1.0f, 1.0f, 0.0f});
                // }
                
                // /* Quadrant 2: highmid -> green */
                // if (!audio_data.history.highmid.empty()) {
                //     led_controller.map_to_leds(led_strip, audio_data.history.highmid, 2, Color{0.0f, 1.0f, 0.0f});
                // }
                
                // /* Quadrant 3: treble -> blue */
                // if (!audio_data.history.treble.empty()) {
                //     led_controller.map_to_leds(led_strip, audio_data.history.treble, 3, Color{0.0f, 0.0f, 1.0f});
                // }
                
                ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                /* Reset flag after displaying to avoid redundant updates */
                audio_complete = false;
            }
            xSemaphoreGive(audio_mutex);
        }
        
        // vTaskDelay(pdMS_TO_TICKS(5));
    }
}
