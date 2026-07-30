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
        bool should_update = false;
        
        /* Check flag and reset with minimal lock time */
        if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            if (audio_complete) {
                should_update = true;
                audio_complete = false;
            }
            xSemaphoreGive(audio_mutex);
        }
        
        /* Perform LED operations using global audio_data directly */
        if (should_update) {
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
            
            /* Map audio data to LEDs using global data */
            led_controller.map_to_leds(led_strip, audio_data.dft.mag, 0, LED_STRIP_LED_NUMBERS, audio_data.theme_colors.ThemeColor0);
            
            /* Refresh LED display */
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
