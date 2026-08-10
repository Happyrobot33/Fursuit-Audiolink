/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cstdio>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_controller.h"
#include "receiver.h"
#include "config.h"

// Global data structures
LEDController led_controller;

// Framerate tracking
static uint32_t frame_count = 0;
static uint32_t last_log_time_ms = 0;
static led_strip_handle_t g_led_strip = nullptr;
static LEDController *g_led_controller = nullptr;

static void receiver_process_task(void *arg) {

    while (true) {
        receiver_test();
    }

    while (1) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        receiver_rf24_poll();
        receiver_process_pending();
    }
}

static void led_update_task(void *arg) {
    while (1) {
        AudiolinkData local_audio_data;

        bool should_update = receiver_take_decoded_frame(local_audio_data, pdMS_TO_TICKS(5));

        if (should_update && g_led_controller && g_led_strip) {
            uint32_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            frame_count++;
            uint32_t elapsed_time_ms = current_time_ms - last_log_time_ms;
            if (elapsed_time_ms >= 1000) {
                float framerate = (frame_count * 1000.0f) / elapsed_time_ms;
                ESP_LOGI(TAG, "Framerate: %.1f FPS", framerate);
                frame_count = 0;
                last_log_time_ms = current_time_ms;
            }

            g_led_controller->clear(g_led_strip);
            // Previous bass-based rendering path (kept for quick fallback/testing):
            // g_led_controller->map_to_leds(g_led_strip,
            //                               local_audio_data.history.bass,
            //                               0,
            //                               LED_STRIP_LED_NUMBERS,
            //                               Color{1.0f, 0.0f, 0.0f}); // Red for bass
            const std::vector<Color> &strip_colors = local_audio_data.colorchord.lights;
            if (!strip_colors.empty()) {
                for (int led_index = 0; led_index < LED_STRIP_LED_NUMBERS; ++led_index) {
                    size_t color_index = (static_cast<size_t>(led_index) * strip_colors.size()) /
                                         static_cast<size_t>(LED_STRIP_LED_NUMBERS);
                    if (color_index >= strip_colors.size()) {
                        color_index = strip_colors.size() - 1;
                    }
                    g_led_controller->set_pixel(g_led_strip, led_index, strip_colors[color_index]);
                }
            }
            ESP_ERROR_CHECK(led_strip_refresh(g_led_strip));
        }

        vTaskDelay(pdMS_TO_TICKS(should_update ? 5 : 15));
    }
}

extern "C" void app_main(void) {
    /* Initialize GPIO and LED strip */
    gpio_set_drive_capability(LED_STRIP_BLINK_GPIO, GPIO_DRIVE_CAP_3);
    led_strip_handle_t led_strip = led_controller.init();
    g_led_strip = led_strip;
    g_led_controller = &led_controller;

    /* Start LED rendering on CPU 1 */
    BaseType_t task_created = xTaskCreatePinnedToCore(
        led_update_task,
        "led_update_task",
        4096,
        nullptr,
        2,
        nullptr,
        1);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED update task");
    }

    /* Initialize RF24 receiver before starting receiver task so test loop has hardware ready. */
    receiver_rf24_init();

    /* Run decompress/decode work on CPU0 to keep CPU1 IDLE alive for task watchdog */
    TaskHandle_t receiver_task_handle = nullptr;
    BaseType_t receiver_task_created = xTaskCreatePinnedToCore(
        receiver_process_task,
        "receiver_process_task",
        8192,
        nullptr,
        2,
        &receiver_task_handle,
        0);
    if (receiver_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receiver process task");
    } else {
        receiver_set_process_task_handle(receiver_task_handle);
    }

    ESP_LOGI(TAG, "Application started - waiting for audio data...");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
