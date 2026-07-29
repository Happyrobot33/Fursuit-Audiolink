/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "audio_processor.h"
#include "led_controller.h"
#include "receiver.h"
#include "config.h"

// Global instances
AudioProcessor audio_processor;
LEDController led_controller;
SemaphoreHandle_t audio_mutex = nullptr;

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
        if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (audio_processor.is_complete) {
                /* Clear LED strip */
                led_controller.clear(led_strip);
                
                /* Map 4 frequency bands to 4 LED quadrants */
                /* Quadrant 0: bass -> red */
                if (!audio_processor.bass.empty()) {
                    led_controller.map_to_leds(led_strip, audio_processor.bass, 0, 255, 0, 0);
                }
                
                /* Quadrant 1: lowmid -> yellow */
                if (!audio_processor.lowmid.empty()) {
                    led_controller.map_to_leds(led_strip, audio_processor.lowmid, 1, 255, 255, 0);
                }
                
                /* Quadrant 2: highmid -> green */
                if (!audio_processor.highmid.empty()) {
                    led_controller.map_to_leds(led_strip, audio_processor.highmid, 2, 0, 255, 0);
                }
                
                /* Quadrant 3: treble -> blue */
                if (!audio_processor.treble.empty()) {
                    led_controller.map_to_leds(led_strip, audio_processor.treble, 3, 0, 0, 255);
                }
                
                ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                
                /* Reset flag after displaying to avoid redundant updates */
                audio_processor.is_complete = false;
            }
            xSemaphoreGive(audio_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
