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
    while (1) {
        /* Block until callback signals a completed frame, with periodic timeout as safeguard. */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        receiver_process_pending();
        vTaskDelay(pdMS_TO_TICKS(1));
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
            // g_led_controller->map_to_leds(g_led_strip,
            //                               local_audio_data.dft.mag,
            //                               0,
            //                               LED_STRIP_LED_NUMBERS,
            //                               local_audio_data.theme_colors.ThemeColor0);
            g_led_controller->map_to_leds(g_led_strip,
                                          local_audio_data.history.bass,
                                          0,
                                          LED_STRIP_LED_NUMBERS,
                                          Color{1.0f, 0.0f, 0.0f}); // Red for bass
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

    /* Run decompress/decode work on CPU0 to keep CPU1 IDLE alive for task watchdog */
    TaskHandle_t receiver_task_handle = nullptr;
    BaseType_t receiver_task_created = xTaskCreatePinnedToCore(
        receiver_process_task,
        "receiver_process_task",
        6144,
        nullptr,
        2,
        &receiver_task_handle,
        0);
    if (receiver_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receiver process task");
    } else {
        receiver_set_process_task_handle(receiver_task_handle);
    }

    /* Initialize WiFi and ESP-NOW */
    receiver_wifi_init();
    receiver_espnow_init();

    ESP_LOGI(TAG, "Application started - waiting for audio data...");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
